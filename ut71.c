#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>


#define FLAG_AC 	(1 << 0)
#define FLAG_DC 	(1 << 1)
#define FLAG_ACDC 	(1 << 2)
#define FLAG_MINUS	(1 << 4)
#define FLAG_AUTO	(1 << 5)
#define FLAG_MANUAL	(1 << 6)



char verbose = 1;

void print_usage (char *progname) {
	printf("Usage: %s -d /dev/hidrawN [-v] [-n samples]\n", progname);
}

int read_packet (int fd, char *pkt_buf, int pkt_len) {
	int len;
	unsigned char buf[pkt_len+1];
	int remaining = pkt_len;

	while ( remaining > 0 && (len = read(fd, buf, remaining + 1)) >= 0) {
		int i;
		
		int rcvd = buf[0] & 7;

		if (rcvd > remaining)
			rcvd = remaining;

		for (i = 0; i < (len - 0); i++) {
			buf[i] &= 0x0f;
		}

		memcpy(pkt_buf + pkt_len - remaining, buf + 1, rcvd);
		remaining -= rcvd;
		
		if (remaining == 0  && (pkt_buf[pkt_len-1] != 0x0a || pkt_buf[pkt_len-2] != 0x0d) ) {
			memmove(pkt_buf, pkt_buf+1, pkt_len-1);
			remaining = 1;
		}
	}

	return remaining;
}

struct range {
	float coe;
	char *unit;
};


struct range getRange(char range, char function, int flags) {
	struct range ret;
	ret.coe = 1;
	switch (function) {
		case 0:
		case 3:
			ret.unit = "V";
			ret.coe = 0.01*0.001;
			break;
		case 1:
		case 2:
			switch (range) {
				case 1:
					ret.coe = 0.0001;
					break;
				case 2:
					ret.coe = 0.001;
					break;
				case 3:
				case 4:
					ret.coe = 0.01;
					break;

			}
			ret.unit = "V";
			break;
		case 4:
			ret.unit = "Ohm";
			switch (range) {
				case 1:
					ret.coe = 0.01;
					break;
				case 2:
					ret.coe = 0.1;
					break;
				case 3:
					ret.coe = 1;
					break;
				case 4:
					ret.coe = 10;
					break;
				case 5:
					ret.coe = 100;
					break;
				case 6:
					ret.coe = 1000;
					break;
			}
			break;
		case 5:
			ret.unit = "F";
			switch (range) {
				case 1:
					ret.coe = 10E-13;
					break;
				case 2:
					ret.coe = 10E-12;
					break;
				case 3:
					ret.coe = 10E-11;
					break;
				case 4:
					ret.coe = 10E-10;
					break;
				case 5:
					ret.coe = 10E-9;
					break;
				case 6:
					ret.coe = 10E-8;
					break;
				case 7:
					ret.coe = 10E-7;
					break;
			}
			break;
		case 6:
			ret.unit = "DEGC";
			ret.coe = 0.1;
			break;
		case 7:
			ret.unit = "A";
			switch (range) {
				case 0:
					ret.coe = 0.01*1E-6;
					break;
				case 1:
					ret.coe = 0.1*1E-6;
					break;
			}
			break;
		case 8:
			ret.unit = "A";
			switch (range) {
				case 0:
					ret.coe = 1E-6;
					break;
				case 1:
					ret.coe = 1E-5;
					break;

				
			}
		case 9:
			ret.unit = "A";
			ret.coe = 1E-3;
			break;
		case 0x0a:
			ret.unit = "BEEP";
			ret.coe = 1;
			break;
		case 0x0b:
			ret.unit = "VFwd";
			ret.coe = 1E-4;
			break;
		case 0x0c:
			if(flags & FLAG_MINUS) {
				ret.unit = "%";
				ret.coe = 0.01;
				break;
			}

			ret.unit = "Hz";
			switch (range) {
				case 0:
					ret.coe = 1E-3;
					break;
				case 1:
					ret.coe = 1E-2;
					break;
				case 2:
					ret.coe = 1E-1;
					break;
				case 3:
					ret.coe = 1;
					break;
				case 4:
					ret.coe = 10;
					break;
				case 5:
					ret.coe = 100;
					break;
				case 6:
					ret.coe = 1000;
					break;
				case 7:
					ret.coe = 10000;
					break;
			}
		case 0x0d:
			ret.unit = "DEGF";
			ret.coe = 0.1;
			break;

	}
	return ret;
}

int parse_packet (char *pkt) {
	char numbers[7];
	
	char rangeVal 	= pkt[5];
	char function 	= pkt[6];
	char state_acdc = pkt[7];
	char state_auto	= pkt[8];

	int i;
	
	int flags = 0;

	switch (state_acdc) {
		case 0x01:
			flags |= FLAG_AC;
			break;
		case 0x02:
			flags |= FLAG_DC;
			break;
		case 0x03:
			flags |= FLAG_ACDC;
			break;
	}


	if (state_auto & 0x04)
		flags |= FLAG_MINUS;



	int npos = 0;
	int pktpos = 0;
	
	struct range rng = getRange(rangeVal, function, flags);

	if (flags & FLAG_MINUS) {
		numbers[0] = '-';
		npos++;
	}

	for (i=0; i<5; i++)
		pkt[i] += '0';


	for (pktpos = 0; pktpos < 5; pktpos++) {
		numbers[npos] = pkt[pktpos];
		npos++;
	}

	numbers[npos] = 0x00;
	

	double result = atof(numbers)*rng.coe;
	
	if (rng.unit == "%" && result <= 0 )
		result *= -1;

	printf("%.12f%s%s%s\n", result, verbose ? " " : "", verbose ? rng.unit : "", 
			(verbose ? ( ( (flags & FLAG_AC) ? "AC" : ( (flags & FLAG_DC) ? "DC" : ( (flags & FLAG_ACDC) ? "AC+DC" : "" ) ) ) ) : ""));


}

int main (int argc, char *argv[]) {
	char *device = NULL;
	int c;
	int fd;
	int baud = 2400;
	int res;
	char devinit[6];
	char pkt[13];
	int samples_rem = -1;

	devinit[0] = 0x0;
	devinit[1] = baud;
	devinit[2] = baud >> 8;
	devinit[3] = baud >> 16;
	devinit[4] = baud >> 24;
	devinit[5] - 0x03;

	while ( (c = getopt(argc, argv, "d:qn:")) != -1 ) {
		switch (c) {
			case 'd':
				device = optarg;
				break;
			case 'q':
				verbose = 0;
				break;
			case 'n':
				samples_rem = atoi(optarg);
				break;
			case '?':
			default:
				print_usage(argv[0]);
				return 1;

		}
	}

	if (device == NULL) {
		print_usage(argv[0]);
		return 1;
	}

	fd = open(device, O_RDWR);

	if (fd < 0) {
		printf("Error: cannot open file %s\n", device);
		return 2;
	}

	res = ioctl(fd, HIDIOCSFEATURE(sizeof(devinit)), devinit);

	if (res < 0) {
		printf("Error: ioctl SFEATURE failed!\n");
		return 3;
	}
	
	memset(pkt, 0, sizeof(pkt));

	while(!read_packet(fd, pkt, 11) && samples_rem != 0) {
		parse_packet(pkt);	
		memset(pkt, 0, sizeof(pkt));
		if (samples_rem != -1 )
			samples_rem--;

	}
	


	close(fd);
	

	return 0;
}
