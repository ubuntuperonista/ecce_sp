/* This file is http://ecce.sourceforge.net/ecce.c
   It is written in reasonably portable C and should
   be easy to compile with any C compiler, eg on Linux:
   cc -o ecce -DWANT_UTF8 ecce.c

   You may need to do: export LC_ALL=en_US.UTF-8

   Although it's reasonable portable C, there are now
   a couple of places where linux filenames are used -
   you may need to make some small changes if compiling
   for Windows or other non-unix systems.  Look for *SYS*
   in the source below.  Feedback portability improvements
   to gtoal@gtoal.com please.  Note that we assume that
   even a linux system is single-user.  The use of
   tmpnam() for saving in an emergency is potentially
   unsafe in a multi-user environment if there are
   bad actors.

   Version 2.10a adds support for more robust embedding
   of ecce in Emacs, by making a version of the "-command"
   parameter (-hex-command) accept a hex string in order
   to pass the ecce command as a parameter while avoiding
   problems such as the use of " characters in the ecce command.


*SYS*
Add this to your ~/.emacs file (or some equivalent for Windows):

;; hex encoding needed below. Came from https://github.com/jwiegley/emacs-release/blob/master/lisp/hex-util.el
(eval-when-compile
  (defmacro num-to-hex-char (num)
    `(aref "0123456789abcdef" ,num))
  )

(defun encode-hex-string (string)
  "Encode octet STRING to hexadecimal string."
  (let* ((len (length string))
         (dst (make-string (* len 2) 0))
         (idx 0)(pos 0))
    (while (< pos len)
      (aset dst idx (num-to-hex-char (/ (aref string pos) 16)))
      (setq idx (1+ idx))
      (aset dst idx (num-to-hex-char (% (aref string pos) 16)))
      (setq idx (1+ idx) pos (1+ pos)))
    dst))

;; Embedded ECCE support

(defun e (ecce_command)
  (interactive "sEcce> ")
  (let (oldpoint (point))
    (setq oldpoint (point))
    (call-process-region (point-min) (point-max)
                         "/bin/bash"
                         t t nil
                         "-c"
                         (concat "ecce - - -hex-command "
                                 (concat (encode-hex-string (concat (concat (format "(r,m)%d(l,m-r0)?\n" (point)) ecce_command)
                                                                    (format "\ni/%%EMACS%dCURSOR%%/\n%%c" (+ 495620 oldpoint))
                                                                    )
                         ) " 2> ~/.ecce-emacs.err"))
    )
    (goto-char (point-min))
    (search-forward (format "%%EMACS%dCURSOR%%" (+ 495620 oldpoint)))
    (replace-match "" nil nil)
  )
)

(global-set-key "\C-e" 'e)

 */

#define VERSION "V2.10b" /* %V */
 static const char *RCS_Version = "$Revision: 1.4 $"; /* only relevant to my home linux /usr/local/src/ecce */
#define DATE "$Date: 2021/11/30 03:55:52 $"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#ifdef WANT_UTF8
/* EXPERIMENTAL SUPPORT FOR UTF-8 - been tested for a few years now, seems robust enough to make default. */
#include <wchar.h>
#include <locale.h>

typedef wint_t ecce_int;
typedef wchar_t ecce_char;
#else
typedef int ecce_int;
typedef char ecce_char;
#define fputwc(x,f) fputc(x,f)
#define fgetwc(f) fgetc(f)
#define WEOF EOF
#endif
/**************************************************/
/*                                                */
/*                                                */
/*                     E C C E                    */
/*                                                */
/*                                                */
/*     ECCE was designed by Hamish Dewar, now     */
/* retired.  This implementation is by Graham     */
/* Toal, of the Edinburgh Computer History        */
/* Project.                                       */
/*                                                */
/* This source is released into the public domain */
/* by the author, without restriction.            */
/*                                                */
/* (c) Graham Toal, 1984. (Original BCPL version, */
/*   translated into C in 1992).                  */
/**************************************************/

/**************************************************************************/


#define NOTE_FILE "/tmp/Note0" /* Specific to Linux - unfortunately, /tmp is shared by other users */
             /* so this version of Ecce is really only expected to be used on a single-user system */

/* #define NOTE_FILE "/dev/shm/Note0" // Specific to the variation of Linux I'm using (ram disk)  *SYS*/

 /* I'm aware that this area of the code needs work to be made robust.  It looks like I can't
   have robustness without some OS-specific code.

   This code is already less portable than was first intended.
   Look for lines containing the marker *SYS* to see if the
   small deviations from portability affect your system.

   Note that the libraries with most windows C compilers try to handle
   some unix constructs such as / as a filename separator, so even some
   of the code marked *SYS* is likely to work on Windows.
*/

              /* Name of temp file for multiple contexts - system dependant. */
              /* Something like "/tmp/Note%c" would be a small improvement,  */
              /* but using a proper function like tmpnam() would be best.    */

              /* Unfortunately tmpnam is deprecated due to timing issues     */
	      /* with setting up file permissions - but it is the only call  */
              /* in this area that is portable to Win/DOS, and I'm trying    */
              /* to keep this source OS-independent. (without ifdef's)       */

              /* This is the remaining code issue I'ld like to fix before    */
              /* moving this to sourceforge.                                 */


#define CONTEXT_OFFSET (strlen(NOTE_FILE)-1)
              /* Index of variable part in name above (i.e. of '0')         */

static char *ProgName = NULL;
static char *parameter[4] = {NULL, NULL, NULL, NULL}; /* parameters - from, to, log, command */
static char *commandp = NULL;

#define    F       0  /* FROM */
#define    T       1  /* TO */
#define    L       2  /* LOG */
#define    C       3  /* COMMAND */

unsigned long estimate_buffer_size(char *fname)
{
  FILE *tmp = fopen(fname, "rw");
  unsigned long maxbuf = 0UL;
  long rc;

  /* since we allocate RAM for the whole file, don't bother handling
     files longer than 32 bits.  It's just a text editor after all... */

  if (tmp == NULL) return 2UL*1024UL*1024UL;
  (void)fseek(tmp, 0L, SEEK_END);
  rc = ftell(tmp);
  if ((rc < 0) || ferror(tmp)) maxbuf = 0UL; else maxbuf = (unsigned long)rc;
  (void)fclose(tmp);
  return (maxbuf + 1024UL*256UL) * 3UL;
}

/**************************************************************************/

#define FALSE (0!=0)
#define TRUE (0==0)

/* Types */

typedef int bool;
typedef ecce_char *cindex;

/* Consts */

#define    bs              8
#define    bell            7
#define    nul             0
#define    del             127
/* The casebit logic only works on 8-bit characters.  Will need to
   rewrite case handling if/when we move to UTF 32-bit encoding */
#define    casebit         ('a'-'A')
#define    minusbit        casebit
#define    plusbit         0x80

/* I know it is bad practise to have these fixed length arrays and I will
   work on that eventually.  I increased the size of these considerably
   when I modified Ecce to accept a command string as a parameter, because
   scripts were starting to need quite long command strings that were
   exceeding the inital bounds of 127 chars.  Again, we're assuming a
   non-hostile single-user environment. */
#define    Max_command_units 4095
#define    Max_parameter     4095
#define    Max_prompt_length 4095

#define    rep             1
#define    txt             2
#define    scope           4
#define    sign            8
#define    delim           16
#define    numb            32
#define    ext             64
#define    err             128
#define    dig             0
#define    pc              1
#define    lpar            2
#define    comma           3
#define    rpar            4
#define    plus            5
#define    minus           6
#define    pling           7
#define    star            8
#define    termin          15

void init_globals (void); 
void free_buffers (void); 
void local_echo (ecce_int *sym);        /* Later, make this a char fn. */
void read_sym (void); 
bool fail_with (char *mess, ecce_int culprit); 
void percent (ecce_int Command_sym); 
void unchain(void); 
void stack(void); 
void execute_command(void); 
void Scan_sign(void);                        /* Could be a macro */
void Scan_scope(void);                       /* ditto macro */
void Scan_text(void); 
void Scan_repeat (void); 
bool analyse (void); 
void load_file (void); 
bool execute_unit (void); 
void execute_all (void); 
ecce_int case_op (ecce_int sym);                /* should be made a macro */
bool right (void); 
bool left (void); 
void right_star(void);                       /* Another macro */
void left_star(void);                        /* Likewise... */
void move (void); 
void move_back(void); 
void move_star (void); 
void move_back_star (void); 
void insert (void); 
void insert_back (void); 
bool verify(void); 
bool verify_back (void); 
bool find (void); 
bool find_back (void);

/* Global variables */

static unsigned long buffer_size = 0UL;
static char *note_file;
static bool  ok;
static bool  printed;
static long  stopper;
static int   max_unit;
static ecce_int pending_sym;

/* significance of file pointers using the 'buffer gap' method: */

/* [NL] o n e NL t w . . . o NL n e x t NL . . NL l a s t NL [NL] */
/*      !        !   !     !  !                                !  */
/*      f        l   p     f  l                                f  */
/*      b        b   p     p  e                                e  */
/*      e        e            n                                n  */
/*      g        g            d                                d  */

/* Note that when the buffer is 100% full, pp and fp are equal,
   and any insertion operations will fail.  This is valid as
   pp is exclusive and fp is inclusive. */

/* When editing a secondary input buffer, these pointers are saved
   and re-created within the buffer gap */

/* Hamish's implementations forced the top part of the buffer out
   to file when the buffer was full (cf 'makespace()'); this isn't
   really an option in today's environment.  Alternative choices
   are:

   1) crash.  (what we did, prior to 2.7)
   2) fail to insert (what we do now)
   3) expand the buffer (realloc, or malloc+free)
      - I don't like this because at some point you do run out
        of RAM or VM, and have to fail anyway.  Since the most
        likely reason this is happening is a bad user command
        (eg (b0)0 ) rather than a file that is genuinely too large,
        I'd prefer to fail on the first instance of it going wrong.
   4) use memory-mapped files (unix has them now too, very similar
        to what we had on EMAS) - but the argument against is just
        a delayed version of (3) above.

   Note that the failure mode of this code is *not* atomic.
   A complete 'get line' or 'insert string' operation would fail
   in Hamish's implementation.  Here it fails on the individual
   character level.  I chose this model primarily to lower the
   cost of the buffer-full test.
 */

static cindex fbeg;
static cindex lbeg;
static cindex pp;
static cindex fp;
static cindex lend;
static cindex fend;

static int   type;
static ecce_int command;
static long  repeat_count;
static long  limit;
static int   pointer;
static int   last_unit;
static int   this_unit;
static int   pos;
static int   endpos;
static ecce_int sym;        /************* sym has to be an int as
                                        it is tested against EOF ************/
static long  number;
static cindex pp_before;
static cindex fp_before;
static cindex ms;
static cindex ms_back;
static cindex ml;
static cindex ml_back;
static int   to_upper_case;
static int   to_lower_case;
static int   caseflip;
static bool  blank_line;
static char *eprompt;
static cindex noted;
static int   changes;
static bool  in_second;
static char *com_prompt;

static int symtype[256] = {
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   ext+termin,          /*NL*/
   err,                 /* */
   ext+numb+7,          /*!*/
   delim,               /*"*/
   err,                 /*#*/
   err,                 /*$*/
   ext+1,               /*%*/
   err,                 /*&*/
   delim,               /*'*/
   ext+2,               /*(*/
   ext+4,               /*)*/
   ext+numb+8,          /***/
   ext+5,               /*+*/
   ext+3,               /*,*/
   ext+6,               /*-*/
   delim,               /*.*/
   delim,               /*slash*/
   ext+numb+0,          /*0*/
   ext+numb+0,          /*1*/
   ext+numb+0,          /*2*/
   ext+numb+0,          /*3*/
   ext+numb+0,          /*4*/
   ext+numb+0,          /*5*/
   ext+numb+0,          /*6*/
   ext+numb+0,          /*7*/
   ext+numb+0,          /*8*/
   ext+numb+0,          /*9*/
   delim,               /*:*/
   ext+15,              /*;*/
   ext+2,               /*<*/
   delim,               /*=*/
   ext+4,               /*>*/
   0,                   /*?*/
   err,                 /*@*/
   scope,               /*A*/
   sign+rep,            /*B*/
   sign+rep,            /*C*/
   sign+scope+txt+rep,  /*D*/
   sign+rep,            /*E*/
   sign+scope+txt+rep,  /*F*/
   sign+rep,            /*G*/
   scope,               /*H*/
   sign+txt+rep,        /*I*/
   sign+rep,            /*J*/
   sign+rep,            /*K*/
   sign+rep,            /*L*/
   sign+rep,            /*M*/
   0,                   /*N*/
   err,                 /*O*/
   sign+rep,            /*P*/
   err,                 /*Q*/
   sign+rep,            /*R*/
   sign+txt,            /*S*/
   sign+scope+txt+rep,  /*T*/
   sign+scope+txt+rep,  /*U*/
   sign+txt,            /*V*/
   err,                 /*W*/
   err,                 /*X*/
   err,                 /*Y*/
   err,                 /*Z*/
   ext+2,               /*[*/
   0,                   /*\*/
   ext+4,               /*]*/
   ext+6,               /*^*/
   delim,               /*_*/
   err,                 /*@*/
   err,                 /*A*/
   sign+rep,            /*B*/
   sign+rep,            /*C*/
   sign+scope+txt+rep,  /*D*/
   sign+rep,            /*E*/
   sign+scope+txt+rep,  /*F*/
   sign+rep,            /*G*/
   err,                 /*H*/
   sign+txt+rep,        /*I*/
   sign+rep,            /*J*/
   sign+rep,            /*K*/
   sign+rep,            /*L*/
   sign+rep,            /*M*/
   err,                 /*N*/
   err,                 /*O*/
   sign+rep,            /*P*/
   err,                 /*Q*/
   sign+rep,            /*R*/
   sign+txt,            /*S*/
   sign+scope+txt+rep,  /*T*/
   sign+scope+txt+rep,  /*U*/
   sign+txt,            /*V*/
   err,                 /*W*/
   err,                 /*X*/
   err,                 /*Y*/
   err,                 /*Z*/
   ext+2,               /*[*/
   0,                   /*\*/
   ext+4,               /*]*/
   ext+6,               /*^*/
   delim                /*_*/
/* May change some of these to delim at users discretion */
 , err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err, 
   err, err, err, err, err, err, err, err
};

static int sym_type(ecce_char c) {
  if ((0 <= c) && (c <= 255)) return symtype[(unsigned int)c];
  return err;
}

static cindex a;
static FILE *main_in;
static FILE *main_out;
static FILE *tty_in;
static FILE *tty_out;
static FILE *log_out;

static ecce_int *com;
static int  *link;
static ecce_char *text;
static long *num;
static long *lim;

/*****************************************************************************/

static int IntSeen = FALSE; /* set asynchronously by signal routine on ^C */

void gotint(int n) {
  (void)n; /* Supress the annoying 'not used' warning message... */
  IntSeen = TRUE;
}

int h(char c) {
  if (('0' <= c) && (c <= '9')) return c - '0';
  if (('A' <= c) && (c <= 'F')) return c - 'A' + 10;
  if (('a' <= c) && (c <= 'f')) return c - 'a' + 10;
  fprintf(stderr, "%s: hex-command parámetro corrupto - char '%c' no es hex\n", ProgName, c);
  exit(1);
}

char *hex_to_ascii(char *hex) {
  static char commandline[Max_parameter], *f, *t;
  if (strlen(hex)/2+3 >= Max_parameter) {
    fprintf(stderr, "%s: hex-command parámetro muy largo.\n", ProgName);
    exit(1);
  }
  f = hex; t = commandline;
  for (;;) {
    int c1,c2;
    if (*f == '\0') break;
    c1 = h(*f++);
    if (*f == '\0') {
      fprintf(stderr, "%s: hex-command parámetro corrupto. (nro. impar de caracteres)\n", ProgName);
      exit(1);
    }
    c2 = h(*f++);
    *t++ = c1<<4 | c2;
  }
  *t = '\0';
  return commandline;
}

char *backup_save;

int main(int argc, char **argv) {
  static char backup_save_buf[256+L_tmpnam+1];
                               /* L_tmpnam on Win/DOS (LCC32) doesn't include path */
  int argno = 1, inoutlog = 0;
  char *s;

#ifdef WANT_UTF8
  /* If your native locale doesn't use UTF-8 encoding 
   * you need to replace the empty string with a
   * locale like "en_US.utf8"
   */
  char *locale = setlocale(LC_ALL, "");
#endif

  backup_save = tmpnam(backup_save_buf);  /*SYS*/

  /* Historical code, not really needed nowadays as
     people only use Windows and Unix variants :-( */
  
  ProgName = argv[0];
  s = strrchr(ProgName, '/');
  if (s == NULL) s = strrchr(ProgName, '\\');
  if (s == NULL) s = strrchr(ProgName, ':');
  if (s == NULL) s = strrchr(ProgName, ']');
  if (s == NULL) s = ProgName; else s = s+1;
  ProgName = malloc(strlen(s)+1); strcpy(ProgName, s);
  s = strchr(ProgName, '.'); if (s != NULL) *s = '\0';

  /* decode argv into parameter[0..3] and buffer_size */
  for (;;) {
    if (argno == argc) break;
    if ((argv[argno][0] == '-') && (argv[argno][1] != '\0')) {
      int offset = 1;
      if (argv[argno][1] == '-') offset += 1;
      if (strcmp(argv[argno]+offset, "desde") == 0) {
        parameter[F] = argv[argno+1];
      } else if (strcmp(argv[argno]+offset, "a") == 0) {
        parameter[T] = argv[argno+1];
      } else if (strcmp(argv[argno]+offset, "log") == 0) {
        parameter[L] = argv[argno+1];
      } else if (strcmp(argv[argno]+offset, "hex-command") == 0) {
        if (parameter[C] != NULL) {
          fprintf(stderr, "%s: solo un -hex-command \"...\" o -comando \"...\" se permite\n", ProgName);
          exit(1);
        }
        parameter[C] = hex_to_ascii(argv[argno+1]); commandp = parameter[C];
      } else if (strcmp(argv[argno]+offset, "command") == 0) {
        if (parameter[C] != NULL) {
          fprintf(stderr, "%s: only one -command \"...\" or -hex-command \"...\" is allowed\n", ProgName);
          exit(1);
        }
        parameter[C] = argv[argno+1]; commandp = parameter[C];
      } else if (strcmp(argv[argno]+offset, "size") == 0) {
        char *buf_size_str, *endptr;
        buf_size_str = argv[argno+1];
        errno = 0;
        buffer_size = strtoul(buf_size_str, &endptr, 10);
        if (errno != 0) {
          fprintf(stderr, "%s: parámetro de tamaño incorrecto '%s'\n", ProgName, buf_size_str);
          exit(1);
	}
        if ((*endptr != '\0') && (endptr[1] == '\0')) {
          /* memo: removed strcasecmp for portability. Also avoiding toupper etc for locale simplification */
          if (*endptr == 'k' || *endptr == 'K') {
            buffer_size *= 1024UL;
	      } else if (*endptr == 'm' || *endptr == 'M') {
            buffer_size *= (1024UL*1024UL);
	  } else {
            fprintf(stderr,
                    "%s: tipo incorrecto de unidad '%s' (se espera %luK o %luM)\n",
                    ProgName, endptr, buffer_size, buffer_size);
            exit(1);
	  }
	}
      } else {
        fprintf (stderr,
                 "%s: Opción desconocida '%s'\n",
		 ProgName, argv[argno]);
        exit(1);
      }       
      if (argv[argno+1] == NULL) argno += 1; else argno += 2;
    } else {
      /* positional parameters */
      parameter[inoutlog++] = argv[argno++];
    }
  }

  if (buffer_size == 0UL) buffer_size = estimate_buffer_size(parameter[F]);
   parameter[F] = argv[1];

   if (parameter[F] == NULL) {
      fprintf (stderr,
         "%s: {-desde} fichero_entrada {{-to} fichero_salida}? {-log fichero}? {-{hex-}comando 'comandos;%%c'} {-tamaño_ bytes}?\n",
          ProgName);
      exit (30);
   }

   IntSeen = FALSE;

   tty_in = stdin;
   tty_out = stderr;

   if ((strcmp(parameter[F], "-") == 0) || (strcmp(parameter[F], "/dev/stdin") == 0)) {  /*SYS*/
      /* If the input file is stdin, you cannot read commands from stdin as well. */
      if (commandp == NULL) {
        fprintf(stderr, "%s: \"-comando '...'\" opción requerida cuando el fichero de entrada es la entrada estándar\n", ProgName); exit(1);
      } 
      main_in = stdin;
      /* What follows is a dirty hack to allow ecce to be used interactively as part of a pipe */
      /* I'm not at all sure this should even be supported */
      tty_in = fopen("/dev/tty", "rb"); /*SYS*/
      if (tty_in) {
	fprintf(stderr, "%s: usando /dev/tty para entrada de comando\n", ProgName);
      } else {
            tty_in = fopen("CON:", "r");
            if (tty_in) { 
	       fprintf(stderr, "%s: usando CON: para entrada de comando\n", ProgName);
	    } else {
               tty_in = fopen("/dev/null", "rb");
               if (tty_in == NULL) tty_in = fopen("NUL:", "rb");
	       fprintf(stderr, "%s: cuidado - no hay cadena de entrado de comando\n", ProgName);
               if (tty_in == NULL) tty_in = stdin; /* It'll be EOF by the time it is used */
	    }
      }
   } else {
      main_in = fopen (parameter[F], "rb");
   }

   if (main_in == NULL) {
      fprintf (stderr, "Fichero \"%s\" no encontrado\n", parameter[F]);
      exit (30);
   }

   if (parameter[L] == NULL) {
      log_out = NULL;
   } else {
      log_out = fopen (parameter[L], "wb");
      if (log_out == NULL) {
         fprintf (stderr, "%s: Cuidado - No puedo crear \"%s\"\n",
          ProgName, parameter[L]);
      }
   }

   init_globals ();

   a[0]           = '\n';
   a[buffer_size] = '\n';

   fprintf (tty_out, "Ecce\n");

   if (main_in != NULL) load_file ();

   signal(SIGINT, &gotint);

   percent ('E'); /* Select either-case searches, case-flipping C command. */
   for (;;) {
      if (analyse ()) {
         printed = FALSE;
         execute_all ();
         command = 'P';
         repeat_count = 1L;
         if (!printed) execute_command ();
      }

      if (IntSeen) {
        signal(SIGINT, &gotint);

        IntSeen = FALSE;
        fprintf(stderr, "* Escape!\n");
      }

   }
}

void init_globals (void) {

   a = malloc ((buffer_size+1) * sizeof(ecce_char));

   note_file = malloc (Max_parameter+1);

   com  = (ecce_int *) malloc ((Max_command_units+1)*sizeof(ecce_int));
   link = (int *) malloc ((Max_command_units+1)*sizeof(int));
   text = (ecce_char *) malloc ((Max_command_units+1) * sizeof(ecce_char));

   num = (long *) malloc ((Max_command_units+1)*sizeof(long));
   lim = (long *) malloc ((Max_command_units+1)*sizeof(long));

   com_prompt = malloc (Max_prompt_length+1);

   if (a == NULL || note_file == NULL || com == NULL ||
    link == NULL || text == NULL || num == NULL || lim == NULL ||
    com_prompt == NULL) {
      fprintf (stderr, "Incapaz de referir espacio de almacenamiento\n");
      free_buffers();
      exit (40);
   }

   fprintf (stderr, "Espacio de Almacén = %d KBytes\n", (int)(buffer_size>>10));


   fbeg = a+1;
   lbeg = fbeg;
   pp = lbeg;
   fp = a+buffer_size;
   lend = fp;
   fend = lend;
   ms = NULL;
   ms_back = NULL;
   stopper = 0 - buffer_size;
   max_unit = -1;
   pending_sym = '\n';
   blank_line = TRUE;

   (void)strcpy (note_file, NOTE_FILE);
   noted = NULL;
   changes = 0;
   in_second = FALSE;
   (void)strcpy (com_prompt, ">");
}

void free_buffers (void) { /* only needed if checking that we have no heap lossage at end */
  if (a) free (a); a = NULL;
  if (lim) free (lim); lim = NULL;
  if (num) free (num); num = NULL;
  if (text) free (text); text = NULL;
  if (link) free (link); link = NULL;
  if (com) free (com); com = NULL;
  if (com_prompt) free (com_prompt); com_prompt = NULL;
  if (note_file) free (note_file); note_file = NULL;
  if (ProgName) free (ProgName); ProgName = NULL;
}

void local_echo (ecce_int *sym) {       /* Later, make this a char fn. */
   ecce_int lsym;

   if (commandp) {
      lsym = *commandp;
      if (lsym == '\0') {lsym = '\n'; commandp = NULL;} else commandp += 1;
      blank_line = (lsym == '\n');
      *sym = lsym;
      if (log_out != NULL) {
         fputwc (lsym, log_out);
      }
      return;
   }

   if (blank_line) {fprintf(tty_out, "%s", eprompt); fflush(tty_out); }    /* stderr usually unbuffered, but flush needed for cygwin */

   lsym = fgetwc (tty_in);
   if (IntSeen) {
     /* Tuned for windows */
     IntSeen = FALSE;
     signal(SIGINT, &gotint);
     lsym = '\n';
     fputwc('^', tty_out); fputwc('C', tty_out); fputwc('\n', tty_out);
   }
   
   if (lsym == WEOF) {

      IntSeen = FALSE;
      signal(SIGINT, SIG_IGN);
      fputwc('\n', tty_out); /* Undo the prompt */

      percent ('c');
      exit (50);
   }

   if (log_out != NULL) {
      fputwc (lsym, log_out);
   }
   blank_line = (lsym == '\n');
   *sym = lsym;
}

void read_sym (void) {
   if (pending_sym == 0) {
      do { local_echo (&sym); } while (sym == ' ');
                               /* Better test wanted for noise */
   } else {
      sym = pending_sym;   /* C has an ungetc() but not very standard... */
      pending_sym = 0;
   }
}

bool fail_with (char *mess, ecce_int culprit) {
 int dirn_sign;

   if (('a' <= culprit) && (culprit <= 'z')) {
      dirn_sign = '-';
   } else {
     if ((culprit & plusbit) != 0) {
        dirn_sign = '+';
     } else {
        dirn_sign = ' ';
     }
   }
   culprit = culprit & (~plusbit);
   if (('A' <= culprit) && (culprit <= 'Z'))
      culprit = culprit | casebit;
   fprintf (stderr, "* %s %lc%c\n", mess, culprit, dirn_sign);
   do { read_sym (); } while (sym_type(sym) != sym_type(';'));
   return (ok = FALSE);
}


void read_item(void) {
   ecce_int saved_digit;
   read_sym ();
   if (isalpha(sym) && islower(sym)) sym = toupper(sym);
   type = sym_type(sym);
   if ((type & ext) == 0) return;

   switch (type & 15) {

      case star:
         number = 0L;
         return;

      case pling:
         number = stopper-1;
         return;

      case dig:
         saved_digit = sym;
         number = 0L;
         do {
            number = (number * 10) + (sym - '0');
            read_sym();
         } while (('0' <= sym) && (sym <= '9'));
         pending_sym = sym;
         sym = saved_digit; /* for printing in errors */
         return;

      default:
         return;
   }
}

void percent (ecce_int Command_sym) {
   static int note_sec = '0'; /* This one MUST be a static */
   cindex P;
   int inoutlog;
   ecce_int sec_no;
   bool file_wanted; /* %s2 or %s2=fred ? */
   char sec_file[256], *sec_filep;
   ok = TRUE;
   if (!isalpha(Command_sym)) {
      (void) fail_with ("letra para", '%');
      return;
   }
   switch (Command_sym) {

      case 'L':
         to_upper_case = ~0;
         to_lower_case = casebit;
/*         to_lower_case = 0; ---- standard ecce */
         caseflip = 0;
         break;

      case 'U':
         to_upper_case = ~casebit;
         to_lower_case = 0;
/*         to_lower_case = casebit; ---- standard ecce */
         caseflip = 0;
         break;

      case 'N':
         to_upper_case = ~0;
         to_lower_case = 0;
         caseflip = casebit;
         break;

      case 'E':
         to_upper_case = ~casebit; /* Only for searches - not in C command */
         to_lower_case = 0;
         caseflip = casebit;
         break;
      case 'V':
         fprintf (tty_out, "Ecce %s", VERSION);
#ifdef WANT_UTF8
         fprintf (tty_out, "/UTF8");
#endif
         fprintf (tty_out, " en C %s\n", DATE+7);
         break;

      case 'W':
	if ((strcmp(parameter[parameter[T] == NULL ? F : T], "-") == 0) ||
            ((parameter[T] != NULL) && (strcmp(parameter[T], "/dev/stdout") == 0))) { /*SYS*/
           fprintf(stderr, "* %%W no está permitido cuando la salida del fichero es stdout\n");
	   break;
	 }
      case 'C':
         do { read_sym (); } while (sym_type(sym) != sym_type(';'));
   
      case 'c':

         if (parameter[T] == NULL) {
            inoutlog = F;         /* So use input file as output file */
         } else {
            inoutlog = T;
         }

         if (in_second) { /* Copied bit */
         /*************** This block is copied from the %S code below;
           it ensures that the main edit buffer is pulled in when closing
           the edit and writing out the file.  This is a quick hack: I
           should change this and the copy in percent('S') so that both
           share the same subroutine ensure_main_edit() *****************/
            FILE *sec_out = fopen (note_file, "wb");
            (void)strcpy (com_prompt, ">");
            if (sec_out == NULL) {
               (void) fail_with ("No puedo guardar contexto", ' ');
               break;
            }
            P = fbeg;
            for (;;) {
               if (P == pp) P = fp;
               if (P == fend) break;
               fputwc (*P++, sec_out);
            }
            fclose (sec_out);
            pp = fbeg - 1;
            fp = fend + 1;
            fbeg = a+1;
            fend = a+buffer_size;
            lbeg = pp;
            do { --lbeg; } while (*lbeg != '\n');
            lbeg++;
            lend = fp;
            while (*lend != '\n') lend++;
            in_second = FALSE;
/*
            if (sec_no == 0) {
               / * do nothing. Else note it and re-select it if this is
                  a percent('W') ! * /
            }
 */
         }  /* End of copied bit */
         if (Command_sym == 'c') {
            parameter[inoutlog] = backup_save;
            main_out = fopen (parameter[inoutlog], "wb");
            if (main_out == NULL) {
               fprintf(stderr,
                       "Lo siento, no puedo guardar su edición (incluso %s ha fallado)\n", backup_save);
               exit(90);
            }
            fprintf (tty_out, "Ecce abandonado: guardando en %s\n", parameter[inoutlog]);
         } else {
           if ((strcmp(parameter[inoutlog], "-") == 0) || (strcmp(parameter[inoutlog], "/dev/stdout") == 0)) /*SYS*/
               main_out = stdout;
            else
               main_out = fopen (parameter[inoutlog], "wb");
            if (main_out == NULL) {
               fprintf (stderr,
                        "No puedo crear \"%s\" - intento guardarlo en %s en su lugar\n",
                        parameter[inoutlog], backup_save);
               main_out = fopen (backup_save, "w");
               if (main_out == NULL) {
                 fprintf(stderr, "Imposible guardar fichero de todos modos. Me rindo. Lo siento!\n");
                 exit(1);
	       }
            } else {
               if (inoutlog == T) {
                  fprintf (tty_out,
                           "Ecce %s a %s completando.\n", parameter[F], parameter[T]);
               } else {
                  fprintf (tty_out, "Ecce %s completando.\n", parameter[F]);
               }
            }
         }

         P = fbeg;
         for (;;) {
            if (P == pp) P = fp;
            if (P == fend) break;
            fputwc (*P++, main_out);
         }
         if (main_out != stdout) fclose (main_out);

         if (Command_sym == 'W') {
            pending_sym = '\n';
            break;
         }

         if (log_out != NULL) {
            fclose (log_out);
         }
/*         fprintf (tty_out, "Ecce complete\n");      */
         free_buffers ();
         exit (0);

      case 'A':
         if (log_out != NULL) {
            fclose (log_out);
         }
         fprintf (stderr, "\nAbortado!\n");
         free_buffers ();
         exit (60);

      case 'S':
         local_echo (&sec_no);
         file_wanted = FALSE;
         if (sym_type(sec_no) == sym_type(';')) {sec_no = 0;}
           /* '\0' means main, '0' means 0,
              so a plain '%s' in secondary input means switch back to
              main and in main means switch to 0. */
         else if (sec_no == '=') {sec_no = '0'; file_wanted = TRUE;}
           /* Here '0' is explicit because we never want to switch to
              main with a '%s=fred' call. */
         else  {
            if (sec_no == '!') {sec_no = '?';}
            else if (sec_no == '=') {sec_no = '0'; file_wanted = TRUE;}
            else if (!(('0' <= sec_no) && (sec_no <= '9'))) {
               (void) fail_with ("%S", sec_no);
               return;
            }
            local_echo (&sym);
            if (sym == '=') {
               file_wanted = TRUE;
            } else if (sym_type(sym) != sym_type(';')) {
               (void) fail_with ("%S?", sym);
               return;
            }
         }
         if (file_wanted) {
           sec_filep = &sec_file[0];
           do {
             read_sym();
             *sec_filep++ = sym;
           } while (sym != '\n');
           *--sec_filep = '\0';
         }
         pending_sym = '\n';
         note_file[CONTEXT_OFFSET] = note_sec;
         if (in_second) {
            FILE *sec_out = fopen (note_file, "wb");
            (void)strcpy (com_prompt, ">");
            if (sec_out == NULL) {
               (void) fail_with ("No puede guardar contexto", ' ');
               return;
            }
            P = fbeg;
            for (;;) {
               if (P == pp) P = fp;
               if (P == fend) break;
               fputwc (*P++, sec_out);
            }
            fclose (sec_out);
            pp = fbeg - 1;
            fp = fend + 1;
            fbeg = a+1;
            fend = a+buffer_size;
            lbeg = pp;
            do { --lbeg; } while (*lbeg != '\n');
            lbeg++;
            lend = fp;
            while (*lend != '\n') lend++;
            in_second = FALSE;
            if (sec_no == 0) {
               return;
            }
         }
         if (sec_no == 0) sec_no = '0';
         note_file[CONTEXT_OFFSET] = sec_no;
         note_sec = sec_no;
         {
            FILE *sec_in = (file_wanted
                             ? fopen (sec_file, "rb")
                             : fopen (note_file, "rb"));
            if (sec_in == NULL) {
               if (file_wanted) {
                  (void) fail_with ("No puede abrir fichero", ' ');
               } else {
                  (void) fail_with ("Context desconocido", sec_no);
               }
               return;
            }
            (void)strcpy (com_prompt, "X>");
            com_prompt[0] = sec_no;
            in_second = TRUE;
            *pp = '\n';

            fbeg = pp + 1;
            fend = fp - 1;
            pp = fbeg;
            fp = fend;
            *fend = '\n';
            lbeg = pp;
            P = pp;
            for (;;) {
               sym = fgetwc(sec_in);
               if (sym == WEOF) break;
               *P++ = sym;
               if (P == fend) {
                  (void) fail_with ("%S corrupto - sin espacio", ' ');
                  fclose (sec_in);
                  return;
               }
            }
            fclose (sec_in);
            while (P != pp) *--fp = *--P;
            lend = fp;
            while (*lend != '\n') lend++;
         }
         break;

      default:
         (void) fail_with ("Porciento", Command_sym);
   }
   do { read_sym(); } while (sym_type(sym) != sym_type(';'));
}

void unchain(void) {
   do {
      pointer = last_unit;
      if (pointer < 0) return;
      last_unit = link[pointer];
      link[pointer] = this_unit;
   } while (com[pointer] != '(');
}

void stack(void) {
   com[this_unit]  = command;
   link[this_unit] = pointer;
   num[this_unit]  = repeat_count;
   lim[this_unit]  = limit;
   this_unit++;
}

void execute_command(void) {
   cindex i;
   ecce_int sym;

   ok = TRUE;
   switch (command & (~plusbit)) {

      case 'p':
      case 'P':
         printed = TRUE;
         i = lbeg;
         for (;;) {
            if (i == noted) {
               fprintf (tty_out, "*** Nota ***");
               if (i == lbeg) fputc ('\n', tty_out);
            }
            if (i == pp) {
               if (i != lbeg) fputc ('^', tty_out);
               i = fp;
            }
            if (i == lend) break;
            sym = *i++;
#ifdef WANT_UTF8
            sym &= 0xffff;
#else
            sym &= 0xff;
#endif
            if (sym > 127) {
	       /* Would use fputwc but it didn't output anything whereas %lc worked OK */
               fprintf (tty_out, "%lc", sym);
            } else if ((sym < 32) || (sym == 127)) {
               fprintf (tty_out, "<%d>", sym);      /* or %2x ? */
            } else fputc (sym, tty_out);
         }
         if (i == fend) fprintf (tty_out, "*** Fin ***");
         fputc ('\n', tty_out);
         if (repeat_count == 1L) return;
         if ((command & minusbit) != 0) {
            move_back (); left_star();
         } else {
            move ();
         }
         return;

      case 'g':
      case 'G':
         local_echo (&sym);

         if (sym == ':') {
            local_echo (&sym);
            pending_sym = sym;
            if (sym != '\n')
               printed = TRUE;
            ok = FALSE;
            return;
         }
         left_star();
         for (;;) {
            if (pp == fp) /* FULL! */ { ok = FALSE; } else *pp++ = sym;
            if (sym == '\n') break;
            local_echo (&sym);
         }
         lbeg = pp;
         if ((command & minusbit) != 0) {
            move_back();
            printed = TRUE;
         }
         return;

      case 'E':
         if (fp == lend) {
            ok = FALSE;
            return;
         }
         if (repeat_count == 0L) {
            fp = lend;
            ok = FALSE;
         } else fp++;
         return;

      case 'e':
         if (pp == lbeg) {
            ok = FALSE;
            return;
         }
         if (repeat_count == 0L) {
            pp = lbeg;
            ok = FALSE;
         } else --pp;
         return;

      case 'C':
         if (fp == lend) {
            ok = FALSE;
            return;
         }
         sym = *fp++;
         if (('a' <= (sym | casebit)) && ((sym | casebit) <= 'z')) {
            if (caseflip != 0) {
               *pp++ = sym ^ casebit;
            } else {
               *pp++ = ((sym ^ casebit) | to_lower_case) & to_upper_case;
            }
         } else {
            *pp++ = sym;
         }
         return;

      case 'c':
         if (pp == lbeg) {
            ok = FALSE;
            return;
         }
         sym = *--pp;
         if (('a' <= (sym | casebit)) && ((sym | casebit) <= 'z')) {
            if (caseflip != 0) {
               *--fp = sym ^ casebit;
            } else {
               *--fp = ((sym ^ casebit) | to_lower_case) & to_upper_case;
            }
         } else {
            *--fp = sym;
         }
         return;

      case 'l':
      case 'R':
         if (repeat_count == 0L) {
            right_star();
            ok = FALSE;
         } else (void) right ();
         ms_back = NULL;
         return;

      case 'r':
      case 'L':
         if (repeat_count == 0L) {
            left_star();
            ok = FALSE;
         } else (void) left ();
         ms = NULL;
         return;

      case 'B':
         if (pp == fp) /* FULL! */ { ok = FALSE; return; }
         *pp++ = '\n';
         lbeg = pp;
         return;

      case 'b':
         if (pp == fp) /* FULL! */ { ok = FALSE; return; }
         *--fp = '\n';
         lend = fp;
         return;

      case 'J':
         right_star();
         if (fp == fend) {
            ok = FALSE;
            return;
         }
         lend = ++fp;
         while (*lend != '\n')
            lend++;
         return;

      case 'j':
         left_star();
         if (pp == fbeg) {
            ok = FALSE;
            return;
         }
         lbeg = --pp;
         do { --lbeg; } while (*lbeg != '\n');
         lbeg++;
         return;

      case 'M':
         if (repeat_count == 0L) {
            move_star();
            ok = FALSE;
         } else {
            move ();
         }
         return;

      case 'm':
         if (repeat_count == 0L) {
            move_back_star();
            ok = FALSE;
         } else {
            move_back(); left_star(); /* retain standard Edinburgh compatibility - my preference would have been to leave cursor at RHS */
         }
         return;

      case 'k':
      case 'K':
         if ((command & minusbit) != 0) {
            move_back();
            if (!ok) return;
         }
         pp = lbeg;
         fp = lend;
         if (lend == fend) {
            ok = FALSE;
            return;
         }
         lend = ++fp ;
         while (*lend != '\n') lend++;
         return;

      case 'V':
         (void) verify ();
         return;

      case 'v':
         (void) verify_back ();
         return;

      case 'F':
         (void) find ();
         return;

      case 'f':
         (void) find_back ();
         return;

      case 'U':
         if (!find ()) return;
         pp = pp_before;
         lbeg = pp;
         do { --lbeg; } while (*lbeg != '\n');
         lbeg++;
         return;

      case 'u':
         if (!find_back ()) return;
         fp = fp_before;
         lend = fp;
         while (*lend != '\n')
            lend++;
         return;

      case 'D':
         if (!find ()) return;
         fp = ml;
         ms = fp;
         return;

      case 'd':
         if (!find_back ()) return;
         pp = ml_back;
         ms_back = pp;
         return;

      case 'T':
         if (!find ()) return;
         while (fp != ml) *pp++ = *fp++;
         return;

      case 't':
         if (!find_back ()) return;
         while (pp != ml_back) *--fp = *--pp;
         return;

      case 'I':
         insert ();
         return;

      case 'i':
         insert_back ();
         return;

      case 's':
      case 'S':
         if (fp == ms) {
            fp = ml;
         } else if (pp == ms_back) {
            pp = ml_back;
         } else {
            ok = FALSE;
            return;
         }
         if ((command & minusbit) != 0) {
            insert_back ();
         } else {
            insert ();
         }
         return;

      case '(':
         num[pointer] = repeat_count;
         repeat_count = 1L;
         return;

      case ')':
         --(num[this_unit]);
         if ((0 != num[this_unit]) && (num[this_unit] != stopper)) {
            this_unit = pointer;
         }
         repeat_count = 1L;
         return;

      case '\\':
         ok = FALSE;
         return;

      case '?':
         return;

      case ',':
         this_unit = pointer - 1;
         return;

      case 'N':
         noted = pp;
         changes = fp-pp;
         return;

      case 'A':
         if ((noted == NULL)
          || (noted >= pp)
          || (changes != fp-pp)) {                    /*BUG*/
            ok = FALSE;
            return;
         }
         note_file[CONTEXT_OFFSET] = lim[this_unit]+'0';
         {
            FILE *note_out = fopen (note_file, "wb");
            cindex p = noted;

            if (note_out == NULL) {
               ok = FALSE;
               return;
            }

            do {
               fputwc (*p++, note_out);
            } while (p != pp);

            fclose (note_out);

            pp = noted;
            lbeg = pp;
            do { --lbeg; } while (*lbeg != '\n');
            lbeg++;
         }
         noted = NULL;
         return;

      case 'H':
         note_file[CONTEXT_OFFSET] = lim[this_unit]+'0';
         {
            FILE *note_in = fopen (note_file, "rb");
            if (note_in == NULL) {
               ok = FALSE;
               return;
            }

            { cindex p = pp;

               for (;;) {
                  sym = fgetwc(note_in);
                  if (sym == WEOF) break;
                  if (p == fp) {
                     ok = FALSE;
                     break;
                  }
                  *p++ = sym;
               }
               pp = p;
            }
            lbeg = pp;
            do { --lbeg; } while (*lbeg != '\n');
            lbeg++;
            fclose (note_in);
         }
         return;

      default:
         (void) fail_with ("Comando desconocido", command);
         return;
   }
}

void Scan_sign(void) {
   read_sym ();
   if (sym_type(sym) == sym_type('+')) {
      command = command | plusbit;
   } else if ((sym_type(sym) == sym_type('-')) &&
            (('A' <= command) && (command <= 'Z'))) {
      command = command | minusbit;
   } else {
      pending_sym = sym;
   }
}

void Scan_scope(void) {                      /* ditto macro */
   ecce_int uppercase_command = command & (~(minusbit | plusbit));
   if ((uppercase_command == 'D') || (uppercase_command == 'U')) number = 1L; else number = 0L;
   read_item ();
   if ((type & numb) == 0) pending_sym = sym;
   limit = number;
   if (('H' == uppercase_command) || (uppercase_command == 'A')) {
      if (!((0L <= limit) && (limit <= 9L))) limit = '?'-'0';
   }
}
 
void Scan_text(void) {
   ecce_int last;

   read_sym ();
   last = sym;
   if ((sym_type(sym) & delim) == 0) {
      pending_sym = sym;
      (void) fail_with ("Texto para", command);
      return;
   }
   if (('a' <= command) && (command <= 'z')) {
      text[endpos] = 0;
      for (;;) {
         local_echo (&sym);
         if (sym == last) break;
         if (sym == '\n') {
            pending_sym = '\n';
            break;
         }
         text[--endpos] = sym;
      }
      pointer = endpos--;
   } else {
      pointer = pos;
      for (;;) {
         local_echo (&sym);
         if (sym == last) break;
         if (sym == '\n') {
            pending_sym = '\n';
            break;
         }
         text[pos++] = sym;
      }
      text[pos++] = 0;
   }
   ok = TRUE;
}

void Scan_repeat (void) {
   number = 1L;
   read_item ();
   if ((type & numb) == 0) pending_sym = sym;
   repeat_count = number;
}

bool analyse (void) {
   int saved_type;

   ok = TRUE;
   pos = 0;
   endpos = Max_command_units;
   this_unit = 0;
   last_unit = -1;
   eprompt = com_prompt;
   do { read_item (); } while (type == sym_type(';'));
   command = sym;
   if (command == '%') {
      read_sym();
      if (sym_type(sym) == sym_type(';')) {
         pending_sym = sym;
         sym = 0;
      }
      percent (((('a' <= sym) && (sym <= 'z')) ? (sym - casebit) : sym  ));
      return (ok = FALSE); /* to inhibit execution */
   }
   if ((type & numb) != 0) {
      if (max_unit > 0) {
         num[max_unit] = number;
      } else {
         return (ok = FALSE);
      }
      read_item();
      if (type != sym_type(';'))
         (void) fail_with ("?", sym);
      pending_sym = sym;
      return (ok);
   }
   for (;;) {  /* on items */
      if ((type & err) != 0) {
         return (fail_with ("Comando", command));
      }
      if ((type & delim) != 0) {
         return (fail_with ("Comando antes", command));
      }
      if ((type & numb) != 0) {
         return (fail_with ("Conteo de repetición inesperado", command));
      }
      limit = 0L;
      pointer = 0;
      repeat_count = 1L;
      if ((type & ext) == 0) {
         saved_type = type;           /* All this needs a tidy-up */
         if ((saved_type & sign) != 0) Scan_sign ();
         if ((saved_type & scope) != 0) Scan_scope ();
         if ((saved_type & txt) != 0) Scan_text ();
         if (!ok) return (ok);
         if ((saved_type & rep) != 0) Scan_repeat ();
         type = saved_type;
      } else {
         switch (type & 15) {

            case termin:
               pending_sym = '\n';  /* for skipping on error */
               unchain ();
               if (pointer >= 0) {
                  return (fail_with ("Faltante", ')'));
               }
               max_unit = this_unit;
               repeat_count = 1L;
               command = ')';
               stack ();
               command = 0;
               stack ();
               return (ok);

            case lpar:
               command = '(';
               pointer = last_unit;
               last_unit = this_unit;
               break;

            case comma:
               command = ',';
               pointer = last_unit;
               last_unit = this_unit;
               break;

            case rpar:
               command = ')';
               Scan_repeat ();
               unchain ();
               if (pointer < 0) {
                  return (fail_with ("Faltante", '('));
               }
               num[pointer] = repeat_count;
               break;
         }
      }
      stack ();
      read_item ();
      command = sym;
   }  /* on items */
}

void load_file (void) {
   cindex p = fbeg;
   ecce_int sym;

   sym = fgetwc(main_in);
   while (sym != WEOF) {
       if (sym != '\r') { /* Ignore CR in CR/LF on DOS/Win */
        *p++ = sym;
        if (p == fend) {
           fprintf (stderr, "* Fichero muy grande!\n");
           percent ('A');
        }
      }

      sym = fgetwc(main_in);
#ifdef WANT_UTF8
      if (errno == EILSEQ) {
	fprintf(stderr, "Se encontró un caracter ancho inválido. Puede necesitar ejecutar: export LC_ALL=en_US.UTF-8\n");  /*SYS*/
	exit(1);                                                
      }
#endif
   }
   fclose (main_in);

   while (p != fbeg) *--fp = *--p;
   lend = fp;
   while (*lend != '\n')
      lend++;
}

bool execute_unit (void) {
   ecce_int culprit;

   command = com[this_unit];
   culprit = command;
   pointer = link[this_unit];

   repeat_count = num[this_unit];
   for (;;) {  /* On repeats of this_unit */
      if (IntSeen) {
        return (ok = FALSE);
      }
      execute_command ();
      --repeat_count;
      if (ok) {
         if (repeat_count == 0L || repeat_count == stopper) {
           return (ok);
         }
         continue;
      }
      ok = TRUE;
      for (;;) {  /* scanning for end of unit (e_g_ ')') */
         if (IntSeen) {
           return (ok = FALSE);
         }
         if (repeat_count < 0L ) {
           if (com[this_unit+1] == '\\') {
              this_unit++;
              return (ok = FALSE);
           }
           return (ok);
         }
         if ((com[this_unit+1] == '\\') || (com[this_unit+1] == '?')) {
            this_unit++;
            return (ok);
         }
         /* indefinite repetition never fails */
         for (;;) {  /* scanning for end of sequence */
            if (IntSeen) {
              return (ok = FALSE);
            }
            this_unit++;
            command = com[this_unit];
            switch (command) {

               case '(':
                  this_unit = link[this_unit];
                  break; /* Skip over (...) as if it were single command. */

               case ',':
                  return (ok);

               case ')': /* Should test for '\\' and '?' following? */
                  --num[this_unit];
                  repeat_count = num[this_unit];
                  /* A bug was fixed here: something got lost in the
                     translation from BCPL to C -- the line below was
                     a 'break' which unfortunately broke out of the
                     enclosing case statement rather than the desired
                     for-loop! */
                  /* rely on enclosing for-loop to handle \ and ? correctly! */
                  goto breaklab;

               default: /* Possible bugfix - what happens on missing cases? */;
            }
            if (com[this_unit] == 0) {/* 0 denotes end of command-line. */
               return (fail_with ("Fallo:", culprit));
            }
         }  /* end of seq */
         breaklab: ;
      }  /* find () ')' without \ or ? */
   } /* executing repeats */
}

void execute_all (void) {
   eprompt = ":";
   this_unit = 0;
   do {
      if (!execute_unit()) {
      	return;
      }
      if (IntSeen) {
        return;
      }
      this_unit++;
   } while (com[this_unit] != 0);
   ok = TRUE;
}

/* All of the following could be static inlines under GCC, or
   I might recode some of them as #define'd macros */

ecce_int case_op (ecce_int sym) {               /* should be made a macro */
   int chr = sym | casebit;
#ifdef NEED_TO_REWRITE_THIS_LATER
   if (isalpha(sym)) { /* this is a flip.  It doesn't yet support %L/%U/%N/%E */
     if (islower(sym)) {
       sym = toupper(sym);
     } else if (isupper(sym)) {
       sym = tolower(sym);
     }
   }
#else
   if (('a' <= chr) && (chr <= 'z')) sym = (sym | to_lower_case)
                                                & to_upper_case;
#endif
   return (sym);
}

bool right (void) {
   if (fp == lend) {
      return (ok = FALSE);
   }
   *pp++ = *fp++;
   return (ok = TRUE);
}

bool left (void) {
   if (pp == lbeg) {
      return (ok = FALSE);
   }
   *--fp = *--pp;
   return (ok = TRUE);
}

void right_star(void) {                      /* Another macro */
   while (fp != lend) *pp++ = *fp++;
}

void left_star(void) {                       /* Likewise... */
   while (pp != lbeg) *--fp = *--pp;
}

void move (void) {
   ok = TRUE;
   right_star ();
   if (fp == fend) {
      ok = FALSE;
      return;
   }
   *pp++ = *fp++;
   lbeg = pp;
   lend = fp;
   while (*lend != '\n') lend++;
   ms_back = NULL;
}

void move_back(void) {
   ok = TRUE;
   left_star ();
   if (pp == fbeg) {
      ok = FALSE;
      return;
   }
   *--fp = *--pp;
   lend = fp;
   lbeg = pp;
   do { --lbeg; } while (*lbeg != '\n');
   lbeg++;
   ms = NULL;
}

void move_star (void) {
   while (fp != fend) *pp++ = *fp++;
   lend = fend;
   lbeg = pp;
   do { --lbeg; } while (*lbeg != '\n');
   lbeg++;
   ms_back = NULL;
}

void move_back_star (void) {
   while (pp != fbeg) *--fp = *--pp;
   lbeg = fbeg;
   lend = fp;
   while (*lend != '\n')
      lend++;
   ms = NULL;
}

void insert (void) {
   int p = pointer;
   ml_back = pp;
   while (text[p] != 0) {
     if (pp == fp) /* FULL! */ { ok = FALSE; break; }
     *pp++ = text[p++];
   }
   ms_back = pp;
   ms = NULL;
}

void insert_back (void) {
   int p = pointer;
   ml = fp;
   while (text[p] != 0) {
     if (pp == fp) /* FULL! */ { ok = FALSE; break; }
     *--fp = text[p++];
   }
   ms = fp;
   ms_back = NULL;
}

bool verify (void) {
   int x = pointer;
   cindex y = fp-1;
   ecce_int if_sym;
   ecce_int sym ;

   do {
      sym = case_op (text[x++]);
      if_sym = case_op (*++y);
   } while (sym == if_sym);

   if (sym != 0) return (ok = FALSE);

   ms = fp;
   ml = y;
   ms_back = NULL;

   return (ok = TRUE);
}

bool verify_back (void) {
   int x = pointer - 1;
   int y = 0;
   ecce_int if_sym;
   ecce_int sym;

   do {
      sym = case_op (text[++x]);
      if_sym = case_op (*(pp - ++y));
   } while (sym == if_sym);

   if (sym != 0) return (ok = FALSE);

   ms_back = pp;
   ml_back = pp - y + 1;
   ms = NULL;

   return (ok = TRUE);
}

bool find (void) {
   ecce_int sym = text[pointer] | casebit;

   pp_before = pp;
   limit = lim[this_unit];
   if (fp == ms) {
      if (!(right ())) move ();
   }
   for (;;) {
      if ((*fp | casebit) == sym) {
         if (verify ()) return (ok);
      }
      if (!right ()) {
         --limit;
         if (limit == 0L) break;
         move ();
         if (!ok) break;
      }
   }

   return (ok = FALSE);
}

bool find_back (void) {
   fp_before = fp;
   limit = lim[this_unit];
   if (pp == ms_back) {
      if (!left ()) move_back ();
   }
   for (;;) {
      if (verify_back ()) return(ok);
      if (!left ()) {
         --limit;
         if (limit == 0L) break;
         move_back ();
         if (!ok) break;
      }
   }

   return (ok = FALSE);
}
