#define _DEFAULT_SOURCE
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <alsa/control.h>
#include <X11/Xlib.h>

#ifdef USE_MPD
#	include <mpd/client.h>
#endif

char* smprintf(const char* fmt, ...) {
	va_list fmtargs;

	va_start(fmtargs, fmt);
	int len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	char* ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

char* get_date_time(const char* fmt, const char* tzname) {
	char buf[129];

	memset(buf, 0, sizeof(buf));
	setenv("TZ", tzname, 1);

	time_t tim = time(NULL);
	struct tm* timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

char* get_battery() {
	FILE *file = NULL;
	if ((file = fopen("/sys/class/power_supply/BAT0/capacity", "r"))) {
		int capacity = 0;
		fscanf(file, "%i\n", &capacity);
		fclose(file);
		if ((file = fopen("/sys/class/power_supply/BAT0/status", "r"))) {
			char *status = malloc(sizeof(char)*12);
			fscanf(file, "%s\n", status);
			fclose(file);
			if (strcmp(status,"Charging") == 0) {
				return smprintf(" %ld%%", capacity);
			} else if (strcmp(status,"Discharging") == 0) {
				if (capacity >= 80) {
					return smprintf(" %ld%%", capacity);
				} else if (capacity >= 60 && capacity < 80) {
					return smprintf(" %ld%%", capacity);
				} else if (capacity >= 40 && capacity < 60) {
					return smprintf(" %ld%%", capacity);
				} else if (capacity >= 20 && capacity < 40) {
					return smprintf(" %ld%%", capacity);
				} else {
					return smprintf(" %ld%%", capacity);
				}
			} else if (strcmp(status,"Full") == 0){
				return smprintf(" %ld%%", capacity);
			}
		}
	}
	return smprintf("");
}

int get_vol() {
	snd_hctl_t* hctl;
	/* To find card and subdevice: /proc/asound/, aplay -L, amixer controls */
	snd_hctl_open(&hctl, "hw:1", 1);
	snd_hctl_load(hctl);

	snd_ctl_elem_id_t* id;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);

	/* amixer controls */
	snd_ctl_elem_id_set_name(id, "Master Playback Volume");

	snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);

	snd_ctl_elem_value_t* control;
	snd_ctl_elem_value_alloca(&control);
	snd_ctl_elem_value_set_id(control, id);

	snd_hctl_elem_read(elem, control);
	int vol = (int)snd_ctl_elem_value_get_integer(control, 0);

	snd_hctl_close(hctl);
	return (vol*100/87);
}

char* get_mpd_current_song() {
#ifdef USE_MPD
	struct mpd_song * song = NULL;
	char* retstr = NULL;
	const char* title = NULL;
	const char* artist = NULL;
	int elapsed = 0, total = 0;
	struct mpd_connection* conn;
	if (!(conn = mpd_connection_new("localhost", 0, 30000)) || mpd_connection_get_error(conn)) {
		return smprintf("");
	}

	mpd_command_list_begin(conn, true);
	mpd_send_status(conn);
	mpd_send_current_song(conn);
	mpd_command_list_end(conn);

	struct mpd_status* theStatus = mpd_recv_status(conn);
	if ((theStatus) && (mpd_status_get_state(theStatus) == MPD_STATE_PLAY)) {
		mpd_response_next(conn);
		song = mpd_recv_song(conn);
		title = smprintf("%s",mpd_song_get_tag(song, MPD_TAG_TITLE, 0));
		artist = smprintf("%s",mpd_song_get_tag(song, MPD_TAG_ARTIST, 0));

		elapsed = mpd_status_get_elapsed_time(theStatus);
		total = mpd_status_get_total_time(theStatus);
		mpd_song_free(song);
		retstr = smprintf("  %.2d:%.2d/%.2d:%.2d %s - %s  ",
		                   elapsed/60, elapsed%60,
		                   total/60, total%60,
		                   artist, title);
		free((char*)title);
		free((char*)artist);
	} else {
		retstr = smprintf("");
	}
	mpd_response_finish(conn);
	mpd_connection_free(conn);
	return retstr;
#else
	return smprintf("");
#endif
}

int main(int argc, char** argv) {
	if (argc > 1) {
		printf("%s - statusline for dwm\n", argv[0]);
		return 1;
	}

	Display* dpy;
	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "%s: cannot open display.\n", argv[0]);
		return 1;
	}

	for (;;usleep(500000)) {
		char* time = get_date_time("%H:%M", "Europe/Moscow");
		char* date = get_date_time("%a %d.%m", "Europe/Moscow");
		char* battery = get_battery();
		char* current_song = get_mpd_current_song();

		char* result = smprintf("%s  %i%%  %s   %s   %s", current_song, get_vol(), battery, date, time);

		XStoreName(dpy, DefaultRootWindow(dpy), result);
		XSync(dpy, False);

		printf("%s\n", result);

		free(time);
		free(date);
		free(battery);
		free(current_song);
		free(result);
	}

	XCloseDisplay(dpy);

	return 0;
}
