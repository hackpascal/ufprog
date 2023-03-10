/* Copyright 2002, Red Hat Inc. - all rights reserved */
/*
FUNCTION
<<getdelim>>---read a line up to a specified line delimiter

INDEX
	getdelim

SYNOPSIS
	#include <stdio.h>
	int getdelim(char **<[bufptr]>, size_t *<[n]>,
                     int <[delim]>, FILE *<[fp]>);

DESCRIPTION
<<getdelim>> reads a file <[fp]> up to and possibly including a specified
delimiter <[delim]>.  The line is read into a buffer pointed to
by <[bufptr]> and designated with size *<[n]>.  If the buffer is
not large enough, it will be dynamically grown by <<getdelim>>.
As the buffer is grown, the pointer to the size <[n]> will be
updated.

RETURNS
<<getdelim>> returns <<-1>> if no characters were successfully read;
otherwise, it returns the number of bytes successfully read.
At end of file, the result is nonzero.

PORTABILITY
<<getdelim>> is a glibc extension.

No supporting OS subroutines are directly required.
*/

/* Modified for Unicode */

#include "_ansi.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ufprog/osdef.h>

#define MIN_LINE_SIZE 4
#define DEFAULT_LINE_SIZE 128

ssize_t
wgetdelim (wchar_t **bufptr,
       size_t *n,
       int delim,
       FILE *fp)
{
  wchar_t *buf;
  wchar_t *ptr;
  size_t newsize, numbytes;
  int pos;
  int ch;
  int cont;

  if (fp == NULL || bufptr == NULL || n == NULL)
    {
      errno = EINVAL;
      return -1;
    }

  buf = *bufptr;
  if (buf == NULL || *n < MIN_LINE_SIZE)
    {
      buf = realloc (*bufptr, DEFAULT_LINE_SIZE * sizeof(wchar_t));
      if (buf == NULL)
        {
	  return -1;
        }
      *bufptr = buf;
      *n = DEFAULT_LINE_SIZE;
    }

  numbytes = *n;
  ptr = buf;

  cont = 1;

  while (cont)
    {
      /* fill buffer - leaving room for nul-terminator */
      while (--numbytes > 0)
        {
          if ((ch = getwc (fp)) == EOF)
            {
	      cont = 0;
              break;
            }
	  else
            {
              *ptr++ = (wchar_t)ch;
              if (ch == delim)
                {
                  cont = 0;
                  break;
                }
            }
        }

      if (cont)
        {
          /* Buffer is too small so reallocate a larger buffer.  */
          pos = (int)(ptr - buf);
          newsize = (*n << 1);
          buf = realloc (buf, newsize * sizeof(wchar_t));
          if (buf == NULL)
            {
              cont = 0;
              break;
            }

          /* After reallocating, continue in new buffer */
          *bufptr = buf;
          *n = newsize;
          ptr = buf + pos;
          numbytes = newsize - pos;
        }
    }

  /* if no input data, return failure */
  if (ptr == buf)
    return -1;

  /* otherwise, nul-terminate and return number of bytes read */
  *ptr = '\0';
  return (ssize_t)(ptr - buf);
}
