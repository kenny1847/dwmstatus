#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <mpd/client.h>
#include <X11/Xlib.h>
#include <alsa/asoundlib.h>
#include <alsa/control.h>

char *tzmsk = "Europe/Moscow";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

/* 
 * TIME SECTION
 */

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	setenv("TZ", tzname, 1);

	tim = time(NULL);
	timtm = localtime(&tim);
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

/*
 * ACPI SECTION
 */

#define BATT_CURRENT_NOW  "/sys/class/power_supply/BAT1/current_now"
#define BATT_CHARGE_NOW   "/sys/class/power_supply/BAT1/charge_now"
#define BATT_CHARGE_FULL  "/sys/class/power_supply/BAT1/charge_full"
#define BATT_CAPACITY     "/sys/class/power_supply/BAT1/capacity"
#define BATT_STATUS       "/sys/class/power_supply/BAT1/status"

char *
getbattery()
{
	long chr_now, chr_full, cur_now = 0;
	int capacity,hh,mm,ss = 0;
	long double time = 0;
	char *status = malloc(sizeof(char)*12);
	char *s = malloc(sizeof(char)*5);
	FILE *fp = NULL;
	if ((fp = fopen(BATT_CHARGE_NOW, "r"))) {
		fscanf(fp, "%ld\n", &chr_now);
		fclose(fp);
		fp = fopen(BATT_CHARGE_FULL, "r");
		fscanf(fp, "%ld\n", &chr_full);
		fclose(fp);
		fp = fopen(BATT_CURRENT_NOW, "r");
		fscanf(fp, "%ld\n", &cur_now);
		fclose(fp);
		fp = fopen(BATT_CAPACITY, "r");
		fscanf(fp, "%i\n", &capacity);
		fclose(fp);
		fp = fopen(BATT_STATUS, "r");
		fscanf(fp, "%s\n", status);
		fclose(fp);
		if (strcmp(status,"Charging") == 0){
			s = "\0";
			time = (long double)(chr_full - chr_now)/(long double)cur_now;
			hh = floor(time);
			mm = fmod(time,1)*60;
			ss = fmod(time*60,1)*60;
			return smprintf("%s %0*i:%0*i:%0*i %ld%%", s, 2, hh, 2, mm, 2, ss, capacity);
		}
		if (strcmp(status,"Discharging") == 0){
			if (capacity >= 80) {
				s = "\0";
			} else if (capacity >= 60 && capacity < 80) {
				s = "\0";
			} else if (capacity >= 40 && capacity < 60) {
				s = "\0";
			} else if (capacity >= 20 && capacity < 40) {
				s = "\0";
			} else {
				s = "\0";
			}
			time = (long double)(chr_now)/(long double)cur_now;
			hh = floor(time);
			mm = fmod(time,1)*60;
			ss = fmod(time*60,1)*60;
			return smprintf("%s %0*i:%0*i:%0*i %ld%%", s, 2, hh, 2, mm, 2, ss, capacity);
		}
		if (strcmp(status,"Full") == 0){
			s = "\0";
			return smprintf("%s %ld%%", s, capacity);
		}
	}
	else return smprintf("");
}

/*
 * ALSA SECTION
 */

int
get_vol(void)
{
	int vol;
	snd_hctl_t *hctl;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_value_t *control;
	
	/* To find card and subdevice: /proc/asound/, aplay -L, amixer controls */
	snd_hctl_open(&hctl, "hw:1", 1);
	snd_hctl_load(hctl);
	
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
	
	/* amixer controls */
	snd_ctl_elem_id_set_name(id, "Master Playback Volume");
	
	snd_hctl_elem_t *elem = snd_hctl_find_elem(hctl, id);
	
	snd_ctl_elem_value_alloca(&control);
	snd_ctl_elem_value_set_id(control, id);
	
	snd_hctl_elem_read(elem, control);
	vol = (int)snd_ctl_elem_value_get_integer(control,0);
	
	snd_hctl_close(hctl);
	return (vol*100/87);
}

/*
 * MPD SECTION
 */

char *
getmpdstat() 
{
	struct mpd_song * song = NULL;
	const char * title = NULL;
	const char * artist = NULL;
	char * retstr = NULL;
	int elapsed = 0, total = 0;
	struct mpd_connection * conn ;
	if (!(conn = mpd_connection_new("localhost", 0, 30000)) ||
		mpd_connection_get_error(conn)){
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
}

int
main(void)
{
	char *status, *tmmsk, *bat, *mpd;
	int  vol;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		tmmsk = mktimes("%H:%M", tzmsk);
		bat   = getbattery();
		vol   = get_vol();
		mpd   = getmpdstat();

		status = smprintf("%s  %i%%  %s   %s", mpd, vol, bat, tmmsk);
		setstatus(status);

		free(tmmsk);
		free(bat);
		free(mpd);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}
