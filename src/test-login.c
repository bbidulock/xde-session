#include "xde-xsession.h"

#include <systemd/sd-login.h>

int
main(int argc, char *argv[])
{
	char **seats = NULL;
	char **sessions = NULL;
	uid_t *uids = NULL;
	char **machines = NULL;
	char **p;
	int i, n;

	sd_get_seats(&seats);
	fprintf(stdout, "Seats:\n");
	for (p = seats; p && *p; p++) {

		fprintf(stdout, "\t%s\n", *p);
		{
			char *session = NULL;
			uid_t uid = 0;

			sd_seat_get_active(*p, &session, &uid);
			fprintf(stdout, "\t\tactive-session: %s (%d)\n", session, (int) uid);
			free(session);
		}
		{
			char **sessions = NULL;
			uid_t *uids = NULL;
			unsigned int n = 0;
			char **s;

			sd_seat_get_sessions(*p, &sessions, &uids, &n);
			fprintf(stdout, "\t\tsessions:\n");
			for (i = 0, s = sessions; s && *s; i++, s++) {
				fprintf(stdout, "\t\t\t%s\n", *s);
				free(*s);
			}
			free(sessions);
			free(uids);
		}
		{
			int ret;

			ret = sd_seat_can_multi_session(*p);
			fprintf(stdout, "\t\tcan multisession: %s\n", ret > 0 ? "true" : (ret < 0 ?  "unknown" : "false"));
			ret = sd_seat_can_tty(*p);
			fprintf(stdout, "\t\tcan tty: %s\n", ret > 0 ? "true" : (ret < 0 ?  "unknown" : "false"));
			ret = sd_seat_can_graphical(*p);
			fprintf(stdout, "\t\tcan graphical: %s\n", ret > 0 ? "true" : (ret < 0 ?  "unknown" : "false"));
		}
		free(*p);
	}
	free(seats);
	sd_get_sessions(&sessions);
	fprintf(stdout, "Sessions:\n");
	for (p = sessions; p && *p; p++) {
		fprintf(stdout, "\t%s\n", *p);
		{
			int ret;

			ret = sd_session_is_active(*p);
			fprintf(stdout, "\t\tactive: %s\n", ret > 0 ? "true" : (ret < 0 ?  "unknown" : "false"));
			ret = sd_session_is_remote(*p);
			fprintf(stdout, "\t\tremote: %s\n", ret > 0 ? "true" : (ret < 0 ?  "unknown" : "false"));
		}
		{
			char *type = NULL;

			sd_session_get_type(*p, &type);
			fprintf(stdout, "\t\ttype: %s\n", type);
			free(type);
		}
		{
			char *class = NULL;

			sd_session_get_class(*p, &class);
			fprintf(stdout, "\t\tclass: %s\n", class);
			free(class);
		}
		{
			char *display = NULL;

			sd_session_get_display(*p, &display);
			fprintf(stdout, "\t\tdisplay: %s\n", display);
			free(display);
		}
		{
			char *service = NULL;

			sd_session_get_service(*p, &service);
			fprintf(stdout, "\t\tservice: %s\n", service);
			free(service);
		}
		{
			char *remote_host = NULL;

			sd_session_get_remote_host(*p, &remote_host);
			fprintf(stdout, "\t\tremote_host: %s\n", remote_host);
			free(remote_host);
		}
		{
			char *remote_user = NULL;

			sd_session_get_remote_user(*p, &remote_user);
			fprintf(stdout, "\t\tremote_user: %s\n", remote_user);
			free(remote_user);
		}
		{
			char *tty = NULL;

			sd_session_get_tty(*p, &tty);
			fprintf(stdout, "\t\ttty: %s\n", tty);
			free(tty);
		}
		{
			uid_t uid = 0;

			sd_session_get_uid(*p, &uid);
			fprintf(stdout, "\t\tuid: %d\n", (int) uid);
		}
		{
			unsigned int vt = -1U;

			sd_session_get_vt(*p, &vt);
			fprintf(stdout, "\t\tvt: %d\n", (int) vt);
		}
		{
			char *state = NULL;

			sd_session_get_state(*p, &state);
			fprintf(stdout, "\t\tstate: %s\n", state);
			free(state);
		}
		free(*p);
	}
	free(sessions);
	if ((n = sd_get_uids(&uids)) < 0)
		n = 0;
	fprintf(stdout, "Users:\n");
	for (i = 0; i < n; i++) {
		fprintf(stdout, "\t%d\n", (int) uids[i]);
	}
	free(uids);
	sd_get_machine_names(&machines);
	fprintf(stdout, "Machines:\n");
	for (p = machines; p && *p; p++) {
		fprintf(stdout, "\t%s\n", *p);
		free(*p);
	}
	free(machines);


	return 0;
}


