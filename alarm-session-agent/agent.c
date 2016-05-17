#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>

#include <systemd/sd-daemon.h>
#include <bundle.h>
#include <dlog.h>
#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>

#include "agent.h"

#define POLLFD_MAX 1

GMainLoop *mainloop;

static int _sock_create(const char *path)
{
	int r;
	int fd;
	struct sockaddr_un sa;

	assert(path && *path);

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd == -1) {
		LOGE("Socket '%s': socket %d", path, errno);
		return -1;
	}

	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, path, sizeof(sa.sun_path));
	sa.sun_path[sizeof(sa.sun_path) - 1] = '\0';

	r = unlink(sa.sun_path);
	if (r == -1 && errno != ENOENT) {
		LOGE("Socket '%s': unlink %d", path, errno);
		close(fd);
		return -1;
	}

	r = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
	if (r == -1) {
		LOGE("Socket '%s': bind %d", path, errno);
		close(fd);
		return -1;
	}

	chmod(sa.sun_path, 0666);

	r = listen(fd, SOMAXCONN);
	if (r == -1) {
		LOGE("Socket '%s': listen %d", path, errno);
		close(fd);
		return -1;
	}

	return fd;
}

static int _get_socket(const char *path)
{
	int n;
	int i;
	int r;
	int fd;

	if (!path || !*path) {
		errno = EINVAL;
		return -1;
	}

	n = sd_listen_fds(0);
	if (n < 0) {
		LOGE("sd_listen_fds: %d", n);
		return -1;
	}

	if (n == 0)
		return _sock_create(path);

	fd = -1;
	for (i = SD_LISTEN_FDS_START; i < SD_LISTEN_FDS_START + n; i++) {
		r = sd_is_socket_unix(i, SOCK_STREAM, -1, path, 0);
		if (r > 0) {
			fd = i;
			break;
		}
	}

	if (fd == -1) {
		LOGE("Socket '%s' is not passed", path);
		return _sock_create(path);
	}

	return fd;
}

static void _send_noti(char *service_name, int alarm_id)
{
	GDBusConnection *conn;
	GError *err = NULL;
	LOGE("send expired message, %s, %d", service_name, alarm_id);

	conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
	if (!conn) {
		LOGE("connection error %s", err->message);
		exit(-1);
	}

	g_dbus_connection_call_sync(conn,
			service_name,
			"/org/tizen/alarm/client",
			"org.tizen.alarm.client",
			"alarm_expired",
			g_variant_new("(is)", alarm_id, service_name),
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			-1,
			NULL,
			NULL);

	g_object_unref(conn);
}

static void flush_data(int fd, uint32_t len)
{
	int r;
	uint32_t s;
	char buf[4096];

	while (len > 0) {
		s = len > sizeof(buf) ? sizeof(buf) : len;

		r = recv(fd, buf, s, 0);
		if (r == -1)
			break;

		len -= r;
	}
}

static gboolean _alarm_agent_main(gint fd, GIOCondition condition,
		gpointer user_data)
{
	int r;
	int len;
	int alarm_id;
	int clifd;
	char *service_name;
	uint8_t *data;
	struct sockaddr sa;
	socklen_t addrlen;
	GVariant *gv;

	addrlen = sizeof(sa);
	clifd = accept(fd, (struct sockaddr *)&sa, &addrlen);
	if (clifd == -1) {
		LOGE("Accept: %d", errno);
		return G_SOURCE_REMOVE;
	}

	r = recv(clifd, &len, sizeof(len), 0);
	if (r <= 0) {
		if (r == 0) {
			LOGE("recv: fd %d closed", clifd);
		} else {
			if (errno != EAGAIN && errno != EINTR)
				LOGE("recv: fd %d errno %d", clifd, errno);
		}
		return G_SOURCE_CONTINUE;
	}

	data = malloc(len);
	if (!data) {
		flush_data(clifd, len);
		return G_SOURCE_CONTINUE;
	}

	r = recv(clifd, data, len, 0);
	if (r <= 0) {
		if (r == 0) {
			LOGE("recv: fd %d closed", clifd);
		} else {
			if (errno != EAGAIN && errno != EINTR)
				LOGE("recv: fd %d errno %d", clifd, errno);
		}

		free(data);
		return G_SOURCE_CONTINUE;
	}

	gv = g_variant_new_from_data(G_VARIANT_TYPE("(is)"),
			data, len, TRUE, NULL, NULL);
	assert(gv);

	g_variant_get(gv, "(i&s)", &alarm_id, &service_name);

	_send_noti(service_name, alarm_id);
	close(clifd);
	return G_SOURCE_CONTINUE;
}

gboolean _timeout_handler(gpointer user_data)
{
	g_main_loop_quit(mainloop);
	return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
	int sock_fd;
	char sockpath[PATH_MAX];

	snprintf(sockpath, sizeof(sockpath), "/run/alarm_agent/%d", getuid());
	LOGE("agent start");

	sock_fd = _get_socket(sockpath);
	if (sock_fd < 0) {
		LOGE("agent pre init failed");
		exit(0);
	}

	mainloop = g_main_loop_new(NULL, FALSE);
	g_unix_fd_add(sock_fd, G_IO_IN, _alarm_agent_main, NULL);
	g_timeout_add_seconds(100, _timeout_handler, NULL);

	g_main_loop_run(mainloop);

	g_main_loop_unref(mainloop);
	close(sock_fd);

	return 0;
}
