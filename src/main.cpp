/*
 *    Author: Nicu Pavel <npavel@mini-box.com>
 *    Copyright (c) 2020 Mini-Box.com
 *    Licensed under the terms of the GNU Lesser General Public License v2.1
 *  	http://www.opensource.org/licenses/lgpl-2.1.php 
 */


#include "devices.h"
#include "version.h"
#include "util.h"
#include "usbhid.h"
#include "HIDDCDCUSB.h"
#include "HIDOpenUPS.h"
#include "HIDOpenUPS2.h"
#include "HIDNUCUPS.h"
#include "HIDInterface.h"

#include <getopt.h>
#include <stdarg.h>
#include <math.h>
#include <strings.h>
#include <ctype.h>
#include <libgen.h>
#include <cerrno>


static const struct deviceId* findDeviceByName(const char *name) 
{
	for(int i = 0; deviceIds[i].name != NULL; i++) {
		if (strcasecmp(name, deviceIds[i].name) == 0) {
			return &deviceIds[i];
		}
	}

	return NULL;
}

static int printDeviceCommands(const struct deviceCmd *cmds, FILE *out) 
{
	int i;

	if (!cmds) {
		return 0;
	}
	fprintf(out, "\n");
	for (i = 0; cmds[i].cmd != NULL; i++) {
		fprintf(out, "\t%s - %s\n", cmds[i].cmd, cmds[i].desc);
	}

	return i;	
}

/* Exit codes: 0 ok, 1 bad arguments/unknown device name, 2 open failed, 3 unknown product id */
int usage(const char *progname, FILE *out, int rc)
{
	fprintf(out,
		"Usage: %s [<options>]\n"
		"Options:\n"
		"  -t, --type <device type>:  Select device model (see below)\n"
		"  -n, --index <n>:           Select nth connected device of this model (default 0)\n"
		"  -i, --import <file>:       Write settings from this file. Warning: Will reboot UPS!\n"
		"  -o, --export <file>:       Dump settings to this file\n"
		"  -c, --comments:            Add comments for each configuration variable to output file\n"
		"  -s, --status:              Only output status, don't read and show configuration variables\n"
		"  -e, --exec <cmd:param1:...>: Execute command\n"
		"  -r, --restart:             Restart UPS only and exit\n"
		"  -b, --bootloader:          Restart UPS to bootloader and exit\n"
		"  -h, --help:                Show this help\n"
		"  -V, --version:             Show version\n"
		"\n",
		progname);

	fprintf(out, "Known device models:\n");
	for(int i = 0; deviceIds[i].name != NULL; i++) {
		fprintf(out, "  %s \t- %s (vid: 0x%04x pid: 0x%04x)\n", deviceIds[i].name, deviceIds[i].desc, deviceIds[i].vendorid, deviceIds[i].productid);
		fprintf(out, "    Available Commands: ");
		if (printDeviceCommands(deviceIds[i].cmds, out) == 0) {
			fprintf(out, " None\n");
		};
	}
	return rc;
}

int main(int argc, char **argv)
{

	int ret, c;
	char *progname = basename(argv[0]);
	char *devicetype = NULL;
	char *infile = NULL;
	char *outfile = NULL;
	bool withComments = false;
	bool withConfiguration = true;
	bool doReset = false;
	bool goBootloader = false;
	char *execCmd = NULL;
	unsigned long index = 0;

	static const struct option longopts[] = {
		{"type", required_argument, NULL, 't'},
		{"index", required_argument, NULL, 'n'},
		{"import", required_argument, NULL, 'i'},
		{"export", required_argument, NULL, 'o'},
		{"exec", required_argument, NULL, 'e'},
		{"comments", no_argument, NULL, 'c'},
		{"status", no_argument, NULL, 's'},
		{"restart", no_argument, NULL, 'r'},
		{"bootloader", no_argument, NULL, 'b'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	if (argc < 2)
		return usage(progname, stderr, 1);

	while ((c = getopt_long(argc, argv, "t:i:o:e:cshrbn:V", longopts, NULL)) != -1) {
		switch (c) {
			
			case 't':
				devicetype = optarg;
				break;
			case 'i':
				infile = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'c':
				withComments = true;
				break;
			case 's':
				withConfiguration = false;
				break;
			case 'e':
				execCmd = optarg;
				break;
			case 'h':
				return usage(progname, stdout, 0);
			case 'V':
				printf("%s version %s\n", progname, VERSION);
				return 0;
			case 'r':
				doReset = true;
				break;
			case 'b':
				goBootloader = true;
				break;
			case 'n':
				errno = 0;
				index = strtoul(optarg, NULL, 10);
				if (errno != 0) {
					perror("error in value for -n");
					return usage(progname, stderr, 1);
				}
				if (index > UINT_MAX) {
					fprintf(stderr, "error in value for -n: too big\n");
					return usage(progname, stderr, 1);
				}
				break;
			default:
				return usage(progname, stderr, 1);
		}
	}

	if (!devicetype) {
		fprintf(stderr, "No device type selected !\n");
		return usage(progname, stderr, 1);
	}

	const struct deviceId *devid = findDeviceByName(devicetype);

	if (devid == NULL) {
		fprintf(stderr, "Device '%s' unknown\n", devicetype);
		return 1;
	} else {
		fprintf(stderr, "%s version %s. Selected device %s: %s\n", progname, VERSION, devid->name, devid->desc);
	}

	USBHID *d = new USBHID(devid->vendorid, devid->productid, devid->max_transfer_size, (unsigned int) index);

	d->open();
	if (!d->isOpened())
	{
		fprintf(stderr, "Failed to open USB device\n");
		return 2;
	}

	HIDInterface *ups;

	if (devid->productid == DCDCUSB_PRODUCT_ID) {
		ups = new HIDDCDCUSB(d);
	} else if (devid->productid == OPENUPS_PRODUCT_ID) {
		ups = new HIDOpenUPS(d);
	} else if (devid->productid == OPENUPS2_PRODUCT_ID) {
		ups = new HIDOpenUPS2(d);
	} else if (devid->productid == NUCUPS_PRODUCT_ID) {
		ups = new  HIDNUCUPS(d);
	} else {
		fprintf(stderr, "Unknown product id !");
		d->close();
		return 3;
	}

	/* After opening wait 100ms before querying the device */
	usleep(1000*100);
	
        if (doReset) {
                ups->restartUPS();
                fprintf(stdout, "Restarted UPS\n");
                return 0;
        }

        if (goBootloader) {
                ups->restartUPSInBootloaderMode();
                fprintf(stdout, "Restarted UPS in Bootloader Mode\n");
                return 0;
        }

	
	ups->GetStatus();
	ups->printValues();	

	if (execCmd) {
		if (ups->executeCommand(execCmd)) {
			fprintf(stdout, "Command executed.\n");
			ups->GetStatus();
			ups->printValues();
		}
	}
	
	if (withConfiguration) {
		fprintf(stdout, "Read configuration\n");
		ups->ReadConfigurationMemory();		
		ups->printConfiguration(withComments);
		if (outfile) {
			ups->varsToFile(outfile, withComments);
			fprintf(stdout, "Saved configuration to file: %s\n", outfile);
		}
	}
			
	if (infile) {
		ups->EraseConfigurationMemory();
		fprintf(stdout, "Erased configuration\n");

		ups->fileToVars(infile);
		fprintf(stdout, "Loaded configuration file: %s\n", infile);
	
		ups->WriteConfigurationMemory();
		fprintf(stdout, "Wrote configuration\n");
	
		//ups->varsToFile("settings.cfg.after", false);	
		ups->restartUPS();
		fprintf(stdout, "Restarted UPS\n");
	}
	
	d->close();
	return 0;
}
