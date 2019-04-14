/* Bidiv: Bidi View

   Converts a text file with bidirectional text (in iso8859-8 or UTF-8
   encoding) into visual 8bit text (iso8859-8) (or UTF-8, depending on locale)
   suitable for viewing on a constant-width-font text terminal (or piped to a
   program like "less").

   Usage: bidiv [options[ [file]...
   (same semantics as cat(1))

   TODO: This should be a general utility which is useful not only to Hebrew:
   it should work for Arabic and other RL languages too.

   Copyright (c) 2001-2006 Nadav Har'El
   License: GNU General Public License version 2

   This is version 1.5. See WHATSNEW file for version history.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fribidi/fribidi.h>

/* In the future, this should be done with autoconf */
#define HAVE_LOCALE

#ifdef HAVE_LOCALE
#include <locale.h>
#include <langinfo.h>
#include <string.h>
#endif

char *progname;

int width=80;
/* auto_basedir=0 doesn't change the base direction (it remains the base_dir
   previously defined by a command line option, or automatically), =1 means
   change the base direction on every newline, and =2 means change it every
   paragraph (where a paragraph starts after a completely empty line.
*/
FriBidiCharType base_dir=FRIBIDI_TYPE_N, prev_base_dir=FRIBIDI_TYPE_LTR;
int auto_basedir=2;
int justify=1;
int out_utf8=0;

void
bidiv(FILE *fp)
{
	char *in, *out;
	FriBidiChar *unicode_in, *unicode_out;
	int len, i, c;
	int rtl_line;

	/* Note: rather than reading the entire line with gets, as we did
	   in the first version of this program, this program reads only
	   "width" character segments. There's a problem though: if we
	   can't decide the base direction from the first width characters
	   of a line (or paragraph), we don't know what to do. If this
	   because a real problem, we should in that case read in more
	   characters (up to some limit) until we do know what to do
	*/
	int newline=1, newpara=1;

	in=(char *)malloc(width+1);
	out=(char *)malloc(width*7+1); /* 7 is the maximum number of
					  bytes in one UTF8 char? */
	/* We use (width+1) not just width, to leave place for a double-
	   width null character, which we might or might not need to write
	   (we don't add one to unicode_in, but it appears that fribidi_log2vis
	   puts one (a single null byte?) in unicode_out). But it can't hurt
	   to be extra safe and leave 2 extra bytes on both. */
	unicode_in=(FriBidiChar *)malloc(sizeof(FriBidiChar)*(width+1));
	unicode_out=(FriBidiChar *)malloc(sizeof(FriBidiChar)*(width+1));

	c=0;
	while(c!=EOF){
		if(auto_basedir==1){
			if(newline){
				if(base_dir!=FRIBIDI_TYPE_N)
					prev_base_dir=base_dir;
				base_dir=FRIBIDI_TYPE_N;
			}
		} else if(auto_basedir==2){
			if(newpara){
				if(base_dir!=FRIBIDI_TYPE_N)
					prev_base_dir=base_dir;
				base_dir=FRIBIDI_TYPE_N;
			}
		}
		/* Get the next segment of a line, up to "width" characters
		   or a newline */
		newpara=0;
		for(len=0; len<width; len++){
			/* Read one character. Currently this can either
			   be one byte either ASCII or ISO8859-8 (224..250)
			   or two bytes of UTF8 (such as Hebrew). Other
			   things (such as 3-byte utf8) are not supported
			   properly. This relative ugliness is because I
			   prefered not to have a seperate "input encoding"
			   option to bidiv. This is enabled when TRY_UTF8
			   is defined
			*/
#define TRY_UTF8
			if((c=getc(fp))==EOF)
				break;
			else if(c=='\r'){
				/* just ignore carriage returns... */
				len--;
				continue;
			} else if(c=='\n'){
				if(newline){
					/* a newline after a new line,
					   i.e., an empty line */
					newpara=1;
				} else {
					newline=1;
				}
				break;
#ifdef TRY_UTF8
			} else if(c>0177 && c<0340) {
				/* UTF8 2 byte character */
				int c1;
				if((c1=getc(fp))==EOF)
					break;
				/* if c1 doesn't make sense as a second utf8
				   character, the whole thing isn't utf8. But
				   c wasn't 8859-8, or we would have noticed
				   that sooner :( So this surely won't work :(
				   Maybe we should assume it's iso8859_1?
				   At least this hack prevents us gobbling two
				   characters as one invalid unicode char.
				   TODO: in this case, maybe turn on a "not
				   hebrew" flag, and stop filtering if both
				   input and output are iso?
				*/
				if(c1<0x80||c1>0xbf){
					ungetc(c1, fp);
					unicode_in[len]=
						fribidi_iso8859_8_to_unicode_c(c);
				} else
				unicode_in[len]=((c & 037) << 6) + (c1 & 077);
				newline=0;
#endif
			} else {
#ifdef TRY_UTF8
				/* 240-256 (0341-0377) - presumably 8859-8.
				   In the future we will have a language
				   option, which will control this (as well
				   as the output encoding). */
				unicode_in[len]=
					fribidi_iso8859_8_to_unicode_c(c);
#else
				in[len]=c;
#endif
				newline=0;
			}
		}
		if(len==0){
			if(newpara){
				putchar('\n');
				continue;
			} else {
			/* this is a case where we had "width" characters
			   followed by a newline; In the previous iteration we
			   outputted these characters, and a newline, and now
			   we come to this newline, and we don't need to output
			   it again!
			*/
				continue;
			}
		}
#ifndef TRY_UTF8
		in[len]='\0';
		fribidi_iso8859_8_to_unicode(in, unicode_in);
#endif

		/* output the line */
		fribidi_log2vis(unicode_in, len, &base_dir,
				unicode_out, NULL, NULL, NULL);
		/* if base_dir is still FRIBIDI_TYPE_N (we haven't found
		   any strong-direction character in this paragraph (or
		   something else - depending on option) - we prefer the
		   base direction of the previous paragraph (prev_base_dir)
		   to the arbitrary FRIBIDI_TYPE_N
		*/
		if(base_dir==FRIBIDI_TYPE_N){
#if 0
			base_dir=prev_base_dir;
			fribidi_log2vis(unicode_in, len, &base_dir,
					unicode_out, NULL, NULL, NULL);
#else
			FriBidiCharType tmp_dir=prev_base_dir;
			fribidi_log2vis(unicode_in, len, &tmp_dir,
					unicode_out, NULL, NULL, NULL);
			rtl_line= (tmp_dir==FRIBIDI_TYPE_RTL);
#endif
		} else if(base_dir==FRIBIDI_TYPE_RTL)
			rtl_line=1;
		else
			rtl_line=0;

		if(out_utf8)
			fribidi_unicode_to_utf8(unicode_out, len,
					     out);
		else
			fribidi_unicode_to_iso8859_8(unicode_out, len,
						     out);
		/* if rtl_line (i.e., base_dir is RL), and we didn't fill the
		   entire width, we need to pad with spaces. Maybe in the
		   future this should be an option.
		*/
		if(justify && rtl_line && len<width)
			for(i=len; i<width; i++)
				putchar(' ');
		puts(out);
	}
	/* Free the memory we have allocated */
	free(in);
	free(out);
	free(unicode_in);
	free(unicode_out);
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *s;
	int i,c;
	int status=0;

	/* TODO: get width from tty setup, COLUMNS variable, and/or
	   command line option */
	progname=argv[0];

	/* The COLUMNS variable (set by zsh and bash, for example)
	   overrides the default 'width'.
	   TODO: also try to read the column number directly from the tty.
	*/
	if((s=getenv("COLUMNS"))){
		i=atoi(s);
		if(i>0)
			width=i;
	}

#ifdef HAVE_LOCALE
	setlocale(LC_CTYPE, "");
	out_utf8= !strcmp(nl_langinfo(CODESET),"UTF-8");
#endif

	/* parse command line options */
	while((c=getopt(argc, argv, "w:lpj"))!=-1){
		switch(c){
		case 'w':
			/* set line width*/
			width=atoi(optarg);
			if(width<=0){
				fprintf(stderr,
				 "%s: width argument must be positive: %s.\n",
					progname, optarg);
				return 2;
			}
			break;
		case 'l':
			/* choose direction every line */
			auto_basedir=1;
			break;
		case 'p':
			/* choose direction every paragraph */
			auto_basedir=2;
			break;
		case 'j':
			/* do not justify (pad with spaces) RTL lines */
			justify=0;
			break;
		case '?': case ':':
			fprintf(stderr,
				"usage: %s [-plj] [-w width] [file]...\n",
				progname);
			return 2;
			break;
		default:
			fprintf(stderr,"internal error...\n");
			return 2;
		}
	}
	argv+=optind;
	argc-=optind;
	
	
	if (argc>0){
		int i;
		for(i=0;i<argc;i++){
			if((fp=fopen(argv[i],"r"))==NULL){
				perror(argv[i]);
				status=1;
			} else {
				bidiv(fp);
				fclose(fp);
			}
		}
	} else {
		bidiv(stdin);
	}
	return status;
}
