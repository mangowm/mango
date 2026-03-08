/*
 * Copy me if you can.
 * by 20h
 */

#ifndef ARG_H__
#define ARG_H__

extern char *argv0;

/* use main(int32_t argc, char *argv[]) */
#define ARGBEGIN                                                               \
	for (argv0 = *argv, argv++, argc--;                                        \
		 argv[0] && argv[0][0] == '-' && argv[0][1]; argc--, argv++) {         \
		char argc_;                                                            \
		char **argv_;                                                          \
		int32_t brk_;                                                          \
		if (argv[0][1] == '-' && argv[0][2] == '\0') {                         \
			argv++;                                                            \
			argc--;                                                            \
			break;                                                             \
		}                                                                      \
		for (brk_ = 0, argv[0]++, argv_ = argv; argv[0][0] && !brk_;           \
			 argv[0]++) {                                                      \
			if (argv_ != argv)                                                 \
				break;                                                         \
			argc_ = argv[0][0];                                                \
			switch (argc_)

/* Handles obsolete -NUM syntax */
#define ARGNUM                                                                 \
	case '0':                                                                  \
	case '1':                                                                  \
	case '2':                                                                  \
	case '3':                                                                  \
	case '4':                                                                  \
	case '5':                                                                  \
	case '6':                                                                  \
	case '7':                                                                  \
	case '8':                                                                  \
	case '9'

#define ARGEND                                                                 \
	}                                                                          \
	}

#define ARGC() argc_

#define ARGNUMF() (brk_ = 1, estrtonum(argv[0], 0, INT_MAX))

#define EARGF(x)                                                               \
	((argv[0][1] == '\0' && argv[1] == NULL)                                   \
		 ? ((x), abort(), (char *)0)                                           \
		 : (brk_ = 1,                                                          \
			(argv[0][1] != '\0') ? (&argv[0][1]) : (argc--, argv++, argv[0])))

#define ARGF()                                                                 \
	((argv[0][1] == '\0' && argv[1] == NULL)                                   \
		 ? (char *)0                                                           \
		 : (brk_ = 1,                                                          \
			(argv[0][1] != '\0') ? (&argv[0][1]) : (argc--, argv++, argv[0])))

#define LNGARG() &argv[0][0]

#endif
