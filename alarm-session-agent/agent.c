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
#include <gio/gio.h>

#include "agent.h"

#define POLLFD_MAX 1

static bool loop_flag = true;

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

static void _send_signal(char *service_name, int alarm_id)
{
	gboolean ret;
	GDBusConnection *conn;
	GError *err = NULL;
	guint owner_id = 0;
	LOGE("send signal, %s, %d", service_name, alarm_id);

	conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
	if (!conn) {
		LOGE("connection error %s", err->message);
		exit(-1);
	}

	owner_id = g_bus_own_name_on_connection(conn, "org.tizen.alarm.manager",
			G_BUS_NAME_OWNER_FLAGS_NONE, NULL, NULL, NULL, NULL);
	if (owner_id == 0) {
		LOGE("Acquiring the own name is failed.");
		exit(-1);
	}

	ret = g_dbus_connection_emit_signal(conn,
			service_name,
			"/org/tizen/alarm/manager",
			"org.tizen.alarm.manager",
			"alarm_expired",
			g_variant_new("(is)", alarm_id, service_name),
			&err);
	if (ret == FALSE)
		LOGE("emit signal failed %s", err->message);

	LOGE("send signal done");

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

static void _alarm_agent_main(int sock_fd)
{
	int r;
	int clifd;
	int len;
	int alarm_id;
	char *service_name;
	uint8_t *data;
	struct sockaddr sa;
	socklen_t addrlen;
	GVariant *gv;

	clifd = accept(sock_fd, (struct sockaddr *)&sa, &addrlen);
	if (clifd == -1) {
		LOGE("Accept: %d", errno);
		return;
	}

	r = recv(clifd, &len, sizeof(len), 0);
	if (r <= 0) {
		if (r == 0) {
			LOGE("recv: fd %d closed", clifd);
		} else {
			if (errno != EAGAIN && errno != EINTR)
				LOGE("recv: fd %d errno %d", clifd, errno);
		}
		return;
	}

	data = malloc(len);
	if (!data) {
		flush_data(clifd, len);
		return;
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
		return;
	}

	gv = g_variant_new_from_data(G_VARIANT_TYPE("(is)"),
			data, len, TRUE, NULL, NULL);
	assert(gv);

	g_variant_get(gv, "(is)", &alarm_id, &service_name);

	_send_signal(service_name, alarm_id);

	loop_flag = false;
}

int main(int argc, char *argv[])
{
	struct pollfd pfds[POLLFD_MAX];
	int sock_fd;
	char sockpath[PATH_MAX];
	int i;
	int loop_cnt;

	snprintf(sockpath, sizeof(sockpath), "/run/alarm_agent/%d", getuid());
	LOGE("agent start");

	sock_fd = _get_socket(sockpath);
	if (sock_fd < 0) {
		LOGE("agent pre init failed");
		exit(0);
	}

	pfds[0].fd = sock_fd;
	pfds[0].events = POLLIN;
	pfds[0].revents = 0;

	loop_cnt = 0;
	while (loop_flag) {
		if (poll(pfds, POLLFD_MAX, 100) < 0)
			continue;

		for (i = 0; i < POLLFD_MAX; i++) {
			if ((pfds[i].revents & POLLIN) != 0)
				_alarm_agent_main(pfds[i].fd);
		}
		loop_cnt++;
		if (loop_cnt > 100)
			loop_flag = false;
	}

	return 0;
}
