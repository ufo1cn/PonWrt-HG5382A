/*
 * camlive - USB UVC camera -> RTSP (RTP/JPEG over TCP) + HTTP MJPEG server
 *
 * 用法: camlive [-d /dev/video0] [-r 640x480] [-f 15] [-p 8554] [-P 8555]
 *
 *   -d  视频设备节点          默认 /dev/video0
 *   -r  分辨率 WxH            默认 640x480
 *   -f  帧率                  默认 15
 *   -p  RTSP 监听端口         默认 8554
 *   -P  HTTP 监听端口         默认 RTSP 端口 + 1
 *
 * 兼容 luci-app-camlive 的启动方式: /usr/bin/camlive -r 640x480 -f 15 -p 8554
 *
 * 只做 MJPEG 直通（绝大多数 UVC 摄像头都支持 MJPG），不转码、无外部依赖。
 * RTSP 使用 RTP/AVP/TCP interleaved，便于放在 NAT 后面远程观看。
 * VLC:     rtsp://<IP>:8554/
 * 浏览器:  http://<IP>:8555/
 */

#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <linux/videodev2.h>

#define MAX_CLIENTS   16
#define MAX_FRAME     (2 * 1024 * 1024)
#define RTP_MTU       1400
#define RTP_PT_JPEG   26
#define REQ_BUF       4096
#define NBUF          4

enum cli_kind  { CLI_RTSP = 0, CLI_HTTP = 1 };
enum cli_state { ST_HANDSHAKE = 0, ST_PLAYING = 1 };

struct client {
	int fd;
	int kind;
	int state;
	int ch_rtp;
	int ch_rtcp;
	int cseq;
	unsigned int ssrc;
	unsigned int last_seq;
	unsigned short rtp_seq;
	unsigned int rtp_ts;
	char req[REQ_BUF];
	int req_len;
};

/* ------------------------------------------------------------------ */
static int g_w = 640, g_h = 480, g_fps = 15;
static int g_rtsp_port = 8554, g_http_port = -1;
static const char *g_dev = "/dev/video0";

static unsigned char *g_jpeg;
static int g_jpeg_len;
static unsigned int g_seq;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_wake[2] = { -1, -1 };
static volatile int g_run = 1;

static int g_vfd = -1;
static void *g_buf[NBUF];
static unsigned int g_buflen[NBUF];
static int g_nbuf;
static struct client g_cli[MAX_CLIENTS];

static void logmsg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fputs("camlive: ", stderr);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	fflush(stderr);
}

static void wake_io(void)
{
	ssize_t rc;

	if (g_wake[1] < 0)
		return;
	rc = write(g_wake[1], "x", 1);
	(void)rc;
}

/* 非阻塞整块写；-2 = 对端太慢，-1 = 连接坏了 */
static int send_nb(int fd, const void *buf, size_t len)
{
	const unsigned char *p = buf;
	size_t sent = 0;

	while (sent < len) {
		ssize_t n = send(fd, p + sent, len - sent,
				 MSG_DONTWAIT | MSG_NOSIGNAL);
		if (n > 0) {
			sent += (size_t)n;
			continue;
		}
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			return -2;
		if (n < 0 && errno == EINTR)
			continue;
		return -1;
	}
	return 0;
}

/* ---------------------- V4L2 ---------------------- */
static int v4l2_setup(void)
{
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	struct v4l2_requestbuffers req;
	int i;

	g_vfd = open(g_dev, O_RDWR | O_NONBLOCK, 0);
	if (g_vfd < 0) {
		logmsg("打开 %s 失败: %s", g_dev, strerror(errno));
		return -1;
	}
	if (ioctl(g_vfd, VIDIOC_QUERYCAP, &cap) < 0) {
		logmsg("VIDIOC_QUERYCAP 失败: %s", strerror(errno));
		return -1;
	}
	logmsg("设备: %s (%s)", (char *)cap.card, (char *)cap.driver);

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = (unsigned int)g_w;
	fmt.fmt.pix.height = (unsigned int)g_h;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	if (ioctl(g_vfd, VIDIOC_S_FMT, &fmt) < 0) {
		logmsg("设置 MJPEG 失败: %s", strerror(errno));
		return -1;
	}
	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
		logmsg("驱动未接受 MJPEG（fourcc=0x%08x），请换支持 MJPG 的摄像头",
		       (unsigned int)fmt.fmt.pix.pixelformat);
		return -1;
	}
	g_w = (int)fmt.fmt.pix.width;
	g_h = (int)fmt.fmt.pix.height;
	logmsg("格式: %dx%d MJPEG, sizeimage=%u", g_w, g_h,
	       (unsigned int)fmt.fmt.pix.sizeimage);

	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = (unsigned int)g_fps;
	if (ioctl(g_vfd, VIDIOC_S_PARM, &parm) < 0)
		logmsg("设置帧率失败（继续）: %s", strerror(errno));

	memset(&req, 0, sizeof(req));
	req.count = NBUF;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(g_vfd, VIDIOC_REQBUFS, &req) < 0) {
		logmsg("VIDIOC_REQBUFS 失败: %s", strerror(errno));
		return -1;
	}
	g_nbuf = req.count < NBUF ? (int)req.count : NBUF;

	for (i = 0; i < g_nbuf; i++) {
		struct v4l2_buffer b;

		memset(&b, 0, sizeof(b));
		b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		b.memory = V4L2_MEMORY_MMAP;
		b.index = (unsigned int)i;
		if (ioctl(g_vfd, VIDIOC_QUERYBUF, &b) < 0) {
			logmsg("VIDIOC_QUERYBUF 失败: %s", strerror(errno));
			return -1;
		}
		g_buflen[i] = b.length;
		g_buf[i] = mmap(NULL, b.length, PROT_READ | PROT_WRITE,
				MAP_SHARED, g_vfd, b.m.offset);
		if (g_buf[i] == MAP_FAILED) {
			logmsg("mmap 失败: %s", strerror(errno));
			g_buf[i] = NULL;
			return -1;
		}
		memset(&b, 0, sizeof(b));
		b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		b.memory = V4L2_MEMORY_MMAP;
		b.index = (unsigned int)i;
		if (ioctl(g_vfd, VIDIOC_QBUF, &b) < 0) {
			logmsg("VIDIOC_QBUF 失败: %s", strerror(errno));
			return -1;
		}
	}

	{
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (ioctl(g_vfd, VIDIOC_STREAMON, &type) < 0) {
			logmsg("VIDIOC_STREAMON 失败: %s", strerror(errno));
			return -1;
		}
	}
	return 0;
}

static void *capture_thread(void *arg)
{
	struct timeval t0;
	int frames = 0;

	(void)arg;

	if (v4l2_setup() < 0) {
		g_run = 0;
		wake_io();
		return NULL;
	}

	gettimeofday(&t0, NULL);
	logmsg("采集已启动");

	while (g_run) {
		struct v4l2_buffer b;
		struct pollfd pfd;
		unsigned int idx;

		pfd.fd = g_vfd;
		pfd.events = POLLIN;
		pfd.revents = 0;
		if (poll(&pfd, 1, 500) <= 0)
			continue;

		memset(&b, 0, sizeof(b));
		b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		b.memory = V4L2_MEMORY_MMAP;
		if (ioctl(g_vfd, VIDIOC_DQBUF, &b) < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			logmsg("VIDIOC_DQBUF 失败: %s", strerror(errno));
			break;
		}
		idx = b.index;

		if (b.bytesused > 0 && b.bytesused <= MAX_FRAME && idx < NBUF) {
			pthread_mutex_lock(&g_lock);
			memcpy(g_jpeg, g_buf[idx], b.bytesused);
			g_jpeg_len = (int)b.bytesused;
			g_seq++;
			pthread_mutex_unlock(&g_lock);
			wake_io();

			if (++frames >= 200) {
				struct timeval t1;
				long ms;

				gettimeofday(&t1, NULL);
				ms = (t1.tv_sec - t0.tv_sec) * 1000L +
				     (t1.tv_usec - t0.tv_usec) / 1000L;
				if (ms > 0)
					logmsg("约 %.1f fps",
					       frames * 1000.0 / (double)ms);
				frames = 0;
				t0 = t1;
			}
		}

		memset(&b, 0, sizeof(b));
		b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		b.memory = V4L2_MEMORY_MMAP;
		b.index = idx;
		if (ioctl(g_vfd, VIDIOC_QBUF, &b) < 0) {
			logmsg("VIDIOC_QBUF 失败: %s", strerror(errno));
			break;
		}
	}

	logmsg("采集线程退出");
	return NULL;
}

/* ---------------------- RTP/JPEG ---------------------- */
static int send_rtp_jpeg(struct client *c, const unsigned char *jpg, int len)
{
	int off = 0;
	int chunk_max = RTP_MTU - 12 - 8;
	unsigned char pkt[RTP_MTU];

	if (chunk_max < 64)
		return -1;

	while (off < len) {
		int n = len - off;
		int last = 0;
		unsigned char il[4];

		if (n > chunk_max)
			n = chunk_max;
		else
			last = 1;

		memset(pkt, 0, 12 + 8);
		pkt[0] = 0x80;					/* V=2, P=0, X=0 */
		pkt[1] = (unsigned char)(last ? (RTP_PT_JPEG | 0x80) : RTP_PT_JPEG);
		pkt[2] = (unsigned char)(c->rtp_seq >> 8);
		pkt[3] = (unsigned char)(c->rtp_seq & 0xff);
		pkt[4] = (unsigned char)(c->rtp_ts >> 24);
		pkt[5] = (unsigned char)(c->rtp_ts >> 16);
		pkt[6] = (unsigned char)(c->rtp_ts >> 8);
		pkt[7] = (unsigned char)(c->rtp_ts & 0xff);
		pkt[8] = (unsigned char)(c->ssrc >> 24);
		pkt[9] = (unsigned char)(c->ssrc >> 16);
		pkt[10] = (unsigned char)(c->ssrc >> 8);
		pkt[11] = (unsigned char)(c->ssrc & 0xff);

		/* RFC 2435 载荷头: type-specific + fragment offset + type/Q/W/H */
		pkt[12] = 0;
		pkt[13] = (unsigned char)((off >> 16) & 0xff);
		pkt[14] = (unsigned char)((off >> 8) & 0xff);
		pkt[15] = (unsigned char)(off & 0xff);
		pkt[16] = 0;			/* type 0 = 4:2:2 */
		pkt[17] = 255;			/* Q: 255 = 用 JPEG 自带量化表 */
		pkt[18] = 0;			/* width  = 0，由 JPEG 决定 */
		pkt[19] = 0;			/* height = 0，由 JPEG 决定 */

		memcpy(pkt + 20, jpg + off, (size_t)n);

		il[0] = '$';
		il[1] = (unsigned char)c->ch_rtp;
		il[2] = (unsigned char)((20 + n) >> 8);
		il[3] = (unsigned char)((20 + n) & 0xff);
		if (send_nb(c->fd, il, 4) != 0)
			return -1;
		if (send_nb(c->fd, pkt, (size_t)(20 + n)) != 0)
			return -1;

		off += n;
		c->rtp_seq++;
	}
	c->rtp_ts += (unsigned int)(90000 / (g_fps > 0 ? g_fps : 15));
	return 0;
}

/* ---------------------- HTTP ---------------------- */
static const char g_index_page[] =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/html; charset=utf-8\r\n"
	"Cache-Control: no-cache\r\n"
	"Connection: close\r\n"
	"\r\n"
	"<!doctype html><html><head><meta name=viewport "
	"content=\"width=device-width,initial-scale=1\"><title>CamLive</title></head>"
	"<body style=\"margin:0;background:#111;color:#ddd;font-family:sans-serif\">"
	"<div style=\"padding:6px\">CamLive MJPEG 推流（RTSP: rtsp://本机IP/"
	"）</div><img src=\"/stream\" style=\"width:100%;height:auto\"></body></html>";

static int http_send_frame(struct client *c, const unsigned char *jpg, int len)
{
	char head[160];
	int n = snprintf(head, sizeof(head),
			 "--frame\r\nContent-Type: image/jpeg\r\n"
			 "Content-Length: %d\r\n\r\n", len);

	if (n <= 0)
		return -1;
	if (send_nb(c->fd, head, (size_t)n) != 0)
		return -1;
	if (send_nb(c->fd, jpg, (size_t)len) != 0)
		return -1;
	if (send_nb(c->fd, "\r\n", 2) != 0)
		return -1;
	return 0;
}

/* ---------------------- RTSP ---------------------- */
static const char *hdr_value(const char *req, const char *name,
			     char *out, int outlen)
{
	const char *p = req;
	int nlen = (int)strlen(name);

	while (p && *p) {
		const char *eol = strchr(p, '\n');
		int linelen = eol ? (int)(eol - p) : (int)strlen(p);

		if (linelen > nlen && strncasecmp(p, name, (size_t)nlen) == 0 &&
		    p[nlen] == ':') {
			const char *v = p + nlen + 1;
			int vlen;

			while (*v == ' ' || *v == '\t')
				v++;
			vlen = linelen - (int)(v - p);
			if (vlen <= 0 || vlen >= outlen)
				return NULL;
			memcpy(out, v, (size_t)vlen);
			out[vlen] = '\0';
			if (vlen > 0 && out[vlen - 1] == '\r')
				out[vlen - 1] = '\0';
			return out;
		}
		if (!eol)
			break;
		p = eol + 1;
	}
	return NULL;
}

static int rtsp_reply(struct client *c, int code, const char *reason,
		      const char *extra)
{
	char buf[1024];
	int n = snprintf(buf, sizeof(buf),
			 "RTSP/1.0 %d %s\r\nCSeq: %d\r\n%s\r\n",
			 code, reason, c->cseq, extra ? extra : "");

	if (n <= 0)
		return -1;
	return send_nb(c->fd, buf, (size_t)n) == 0 ? 0 : -1;
}

static int rtsp_describe(struct client *c)
{
	char sdp[1024];
	char head[256];
	int n, hn;

	n = snprintf(sdp, sizeof(sdp),
		     "v=0\r\n"
		     "o=- %u 1 IN IP4 0.0.0.0\r\n"
		     "s=CamLive UVC\r\n"
		     "c=IN IP4 0.0.0.0\r\n"
		     "t=0 0\r\n"
		     "a=control:*\r\n"
		     "m=video 0 RTP/AVP %d\r\n"
		     "a=rtpmap:%d JPEG/90000\r\n"
		     "a=framerate:%d\r\n"
		     "a=control:streamid=0\r\n",
		     c->ssrc, RTP_PT_JPEG, RTP_PT_JPEG, g_fps);
	if (n <= 0)
		return -1;

	hn = snprintf(head, sizeof(head),
		      "RTSP/1.0 200 OK\r\n"
		      "CSeq: %d\r\n"
		      "Content-Base: rtsp://0.0.0.0:%d/\r\n"
		      "Content-Type: application/sdp\r\n"
		      "Content-Length: %d\r\n\r\n",
		      c->cseq, g_rtsp_port, n);
	if (hn <= 0)
		return -1;
	if (send_nb(c->fd, head, (size_t)hn) != 0)
		return -1;
	return send_nb(c->fd, sdp, (size_t)n) == 0 ? 0 : -1;
}

static void rtsp_parse_transport(struct client *c, const char *req)
{
	char v[256];
	const char *p;

	if (!hdr_value(req, "Transport", v, sizeof(v)))
		return;
	p = strstr(v, "interleaved=");
	if (!p)
		return;
	p += 12;
	{
		int a = 0, b = 0;

		if (sscanf(p, "%d-%d", &a, &b) == 2) {
			c->ch_rtp = a;
			c->ch_rtcp = b;
		} else if (sscanf(p, "%d", &a) == 1) {
			c->ch_rtp = a;
			c->ch_rtcp = a + 1;
		}
	}
}

static int rtsp_handle(struct client *c)
{
	char method[32];
	char v[64];
	const char *p = c->req;

	if (sscanf(p, "%31s", method) != 1)
		return -1;

	c->cseq = 1;
	if (hdr_value(c->req, "CSeq", v, sizeof(v)))
		c->cseq = atoi(v);

	if (strcmp(method, "OPTIONS") == 0)
		return rtsp_reply(c, 200, "OK",
				  "Public: OPTIONS, DESCRIBE, SETUP, PLAY, "
				  "PAUSE, TEARDOWN, GET_PARAMETER\r\n");

	if (strcmp(method, "DESCRIBE") == 0)
		return rtsp_describe(c);

	if (strcmp(method, "SETUP") == 0) {
		char extra[192];

		rtsp_parse_transport(c, c->req);
		snprintf(extra, sizeof(extra),
			 "Transport: RTP/AVP/TCP;unicast;interleaved=%d-%d\r\n"
			 "Session: %08X\r\n", c->ch_rtp, c->ch_rtcp, c->ssrc);
		return rtsp_reply(c, 200, "OK", extra);
	}

	if (strcmp(method, "PLAY") == 0) {
		char extra[96];

		snprintf(extra, sizeof(extra),
			 "Session: %08X\r\nRange: npt=0.000-\r\n", c->ssrc);
		c->state = ST_PLAYING;
		c->last_seq = g_seq;
		return rtsp_reply(c, 200, "OK", extra);
	}

	if (strcmp(method, "PAUSE") == 0)
		return rtsp_reply(c, 200, "OK", NULL);

	if (strcmp(method, "TEARDOWN") == 0) {
		(void)rtsp_reply(c, 200, "OK", NULL);
		return -1;
	}

	/* GET_PARAMETER / SET_PARAMETER 当作保活 */
	return rtsp_reply(c, 200, "OK", NULL);
}

static int http_handle(struct client *c)
{
	if (strstr(c->req, "GET /stream") || strstr(c->req, "GET /?action=stream")) {
		if (send_nb(c->fd,
			    "HTTP/1.1 200 OK\r\n"
			    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
			    "Cache-Control: no-cache\r\n"
			    "Pragma: no-cache\r\n"
			    "Connection: close\r\n\r\n", 150) != 0)
			return -1;
		c->state = ST_PLAYING;
		c->last_seq = g_seq;
		return 0;
	}
	(void)send_nb(c->fd, g_index_page, strlen(g_index_page));
	return -1;
}

/* ---------------------- 客户端 ---------------------- */
static int listen_tcp(int port)
{
	int fd, on = 1;
	struct sockaddr_in a;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_ANY);
	a.sin_port = htons((unsigned short)port);
	if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
		logmsg("端口 %d 绑定失败: %s", port, strerror(errno));
		close(fd);
		return -1;
	}
	if (listen(fd, 4) < 0) {
		close(fd);
		return -1;
	}
	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
	return fd;
}

static void cli_close(int i)
{
	if (g_cli[i].fd >= 0)
		close(g_cli[i].fd);
	memset(&g_cli[i], 0, sizeof(g_cli[i]));
	g_cli[i].fd = -1;
}

static void accept_new(int lfd, int kind)
{
	for (;;) {
		int i, fd = accept(lfd, NULL, NULL);
		int on = 1;

		if (fd < 0)
			return;
		for (i = 0; i < MAX_CLIENTS; i++)
			if (g_cli[i].fd < 0)
				break;
		if (i == MAX_CLIENTS) {
			close(fd);
			continue;
		}
		memset(&g_cli[i], 0, sizeof(g_cli[i]));
		g_cli[i].fd = fd;
		g_cli[i].kind = kind;
		g_cli[i].ch_rtp = 0;
		g_cli[i].ch_rtcp = 1;
		g_cli[i].ssrc = 0x12345678u + (unsigned int)i * 7919u;
		fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
		logmsg("新客户端 fd=%d %s", fd, kind == CLI_RTSP ? "RTSP" : "HTTP");
	}
}

static void push_frame(const unsigned char *jpg, int len)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++) {
		struct client *c = &g_cli[i];

		if (c->fd < 0 || c->state != ST_PLAYING)
			continue;
		if (c->last_seq == g_seq)
			continue;
		c->last_seq = g_seq;
		if (c->kind == CLI_RTSP) {
			if (send_rtp_jpeg(c, jpg, len) < 0)
				cli_close(i);
		} else {
			if (http_send_frame(c, jpg, len) < 0)
				cli_close(i);
		}
	}
}

int main(int argc, char **argv)
{
	pthread_t cap;
	unsigned char *snap;
	int lrtsp, lhttp = -1;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d") && i + 1 < argc) {
			g_dev = argv[++i];
		} else if (!strcmp(argv[i], "-r") && i + 1 < argc) {
			int w = 0, h = 0;

			if (sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
				g_w = w;
				g_h = h;
			}
		} else if (!strcmp(argv[i], "-f") && i + 1 < argc) {
			g_fps = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-p") && i + 1 < argc) {
			g_rtsp_port = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-P") && i + 1 < argc) {
			g_http_port = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			printf("用法: camlive [-d /dev/video0] [-r 640x480] "
			       "[-f 15] [-p 8554] [-P 8555]\n");
			return 0;
		}
	}

	if (g_fps <= 0 || g_fps > 60)
		g_fps = 15;
	if (g_rtsp_port <= 0 || g_rtsp_port > 65535)
		g_rtsp_port = 8554;
	if (g_http_port <= 0 || g_http_port > 65535)
		g_http_port = g_rtsp_port + 1;

	signal(SIGPIPE, SIG_IGN);

	g_jpeg = malloc(MAX_FRAME);
	snap = malloc(MAX_FRAME);
	if (!g_jpeg || !snap) {
		logmsg("内存不足");
		return 1;
	}
	g_jpeg_len = 0;
	g_seq = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
		g_cli[i].fd = -1;

	if (pipe(g_wake) < 0) {
		logmsg("pipe 失败: %s", strerror(errno));
		return 1;
	}

	lrtsp = listen_tcp(g_rtsp_port);
	if (lrtsp < 0)
		return 1;
	lhttp = listen_tcp(g_http_port);
	if (lhttp < 0) {
		logmsg("HTTP 端口 %d 不可用，仅提供 RTSP", g_http_port);
		lhttp = -1;
	}

	logmsg("RTSP: rtsp://<本机IP>:%d/   HTTP: http://<本机IP>:%d/",
	       g_rtsp_port, lhttp >= 0 ? g_http_port : g_rtsp_port);

	if (pthread_create(&cap, NULL, capture_thread, NULL) != 0) {
		logmsg("创建采集线程失败");
		return 1;
	}

	while (g_run) {
		struct pollfd pfd[MAX_CLIENTS + 3];
		int slot[MAX_CLIENTS + 3];
		int nfd = 0, rts, hts = -1, tres, k;

		pfd[nfd].fd = g_wake[0];
		pfd[nfd].events = POLLIN;
		pfd[nfd].revents = 0;
		slot[nfd++] = -1;

		pfd[nfd].fd = lrtsp;
		pfd[nfd].events = POLLIN;
		pfd[nfd].revents = 0;
		rts = nfd;
		slot[nfd++] = -2;

		if (lhttp >= 0) {
			pfd[nfd].fd = lhttp;
			pfd[nfd].events = POLLIN;
			pfd[nfd].revents = 0;
			hts = nfd;
			slot[nfd++] = -3;
		}

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (g_cli[i].fd >= 0) {
				pfd[nfd].fd = g_cli[i].fd;
				pfd[nfd].events = POLLIN;
				pfd[nfd].revents = 0;
				slot[nfd++] = i;
			}
		}

		tres = poll(pfd, (unsigned short)nfd, 1000);
		if (tres < 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		for (k = 0; k < nfd; k++) {
			if (pfd[k].revents == 0)
				continue;

			if (slot[k] == -1) {
				char tmp[64];

				while (read(g_wake[0], tmp, sizeof(tmp)) > 0)
					;
				pthread_mutex_lock(&g_lock);
				if (g_jpeg_len > 0) {
					memcpy(snap, g_jpeg, (size_t)g_jpeg_len);
					pthread_mutex_unlock(&g_lock);
					push_frame(snap, g_jpeg_len);
					continue;
				}
				pthread_mutex_unlock(&g_lock);
				continue;
			}

			if (slot[k] == -2) {
				accept_new(lrtsp, CLI_RTSP);
				continue;
			}
			if (slot[k] == -3) {
				accept_new(lhttp, CLI_HTTP);
				continue;
			}

			{
				int ci = slot[k];
				struct client *c = &g_cli[ci];
				unsigned char buf[512];
				int r, need;

				need = REQ_BUF - 1 - c->req_len;
				if (need <= 0) {
					cli_close(ci);
					continue;
				}
				r = (int)recv(c->fd, buf, sizeof(buf), MSG_DONTWAIT);
				if (r <= 0) {
					if (r == 0 || (errno != EAGAIN && errno != EINTR))
						cli_close(ci);
					continue;
				}
				if (r > need)
					r = need;
				memcpy(c->req + c->req_len, buf, (size_t)r);
				c->req_len += r;
				c->req[c->req_len] = '\0';

				if (c->kind == CLI_RTSP && c->state == ST_PLAYING) {
					/* PLAY 之后只可能有保活/心跳，忽略内容 */
					c->req_len = 0;
					c->req[0] = '\0';
					continue;
				}

				if (strstr(c->req, "\r\n\r\n") || strstr(c->req, "\n\n")) {
					if (c->kind == CLI_RTSP) {
						if (rtsp_handle(c) < 0)
							cli_close(ci);
						else {
							c->req_len = 0;
							c->req[0] = '\0';
						}
					} else {
						if (http_handle(c) < 0)
							cli_close(ci);
					}
				}
			}
		}
	}

	logmsg("准备退出");
	g_run = 0;
	pthread_join(cap, NULL);
	if (g_vfd >= 0) {
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		(void)ioctl(g_vfd, VIDIOC_STREAMOFF, &type);
		close(g_vfd);
	}
	for (i = 0; i < MAX_CLIENTS; i++)
		if (g_cli[i].fd >= 0)
			close(g_cli[i].fd);
	close(lrtsp);
	if (lhttp >= 0)
		close(lhttp);
	free(g_jpeg);
	free(snap);
	return 0;
}
