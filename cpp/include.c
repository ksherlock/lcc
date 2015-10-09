#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "cpp.h"

Includelist	includelist[NINCLUDE];

struct hash_entry {
	const char *name;
	unsigned long hash;
	struct hash_entry *next;
};

#define HASH_TABLE_SIZE 32
static struct hash_entry *hash_table[HASH_TABLE_SIZE] = {};


unsigned long hash ( const unsigned char *s )
{
	unsigned long   h = 0, high;
	while ( *s ) {
		h = ( h << 4 ) + *s++;
		if ( high = h & 0xF0000000 )
			h ^= high >> 24;
		h &= ~high;
	}
	return h;
}

/*
 * returns 1 if in hash table
 * returns 0 if not.
 */
static int check_hash(const char *cp, int insert) {
	struct hash_entry *e;
	unsigned long h;
	unsigned index;

	char buffer[PATH_MAX];

	cp = realpath(cp, buffer);
	if (!cp) return 0;

	h = hash(cp);

	index = h % HASH_TABLE_SIZE;
	e = hash_table[index];

	while (e) {
		if (e->hash == h && strcmp(e->name, cp) == 0) return 1;
		e = e->next;
	}
	if (insert) {
		e = domalloc(sizeof(struct hash_entry));
		e->name = strdup(cp);
		e->hash = h;
		e->next = hash_table[index];
		hash_table[index] = e;
	}
	return 0;
}

extern char	*objname;

void
dopragma(Tokenrow *trp)
{
	/* checks #pragma once, ignores all others. */

	trp->tp += 1;

	if (trp->tp->type==NAME 
		&& trp->lp - trp->bp == 4 
		&& strncmp(trp->tp->t, "once", trp->tp->len) == 0
		&& cursource->fd) {
		check_hash(cursource->filename, 1);
		trp->tp += 1;
		setempty(trp);
	}

}


void
doinclude(Tokenrow *trp, int type)
{
	char fname[256], iname[256];
	Includelist *ip;
	int angled, len, i;
	FILE *fd;

	trp->tp += 1;
	if (trp->tp>=trp->lp)
		goto syntax;
	if (trp->tp->type!=STRING && trp->tp->type!=LT) {
		len = trp->tp - trp->bp;
		expandrow(trp, "<include>", Notinmacro);
		trp->tp = trp->bp+len;
	}
	if (trp->tp->type==STRING) {
		len = trp->tp->len-2;
		if (len > sizeof(fname) - 1)
			len = sizeof(fname) - 1;
		strncpy(fname, (char*)trp->tp->t+1, len);
		angled = 0;
	} else if (trp->tp->type==LT) {
		len = 0;
		trp->tp++;
		while (trp->tp->type!=GT) {
			if (trp->tp>trp->lp || len+trp->tp->len+2 >= sizeof(fname))
				goto syntax;
			strncpy(fname+len, (char*)trp->tp->t, trp->tp->len);
			len += trp->tp->len;
			trp->tp++;
		}
		angled = 1;
	} else
		goto syntax;
	trp->tp += 2;
	if (trp->tp < trp->lp || len==0)
		goto syntax;
	fname[len] = '\0';
	if (fname[0]=='/') {
		fd = fopen(fname, "r");
		strcpy(iname, fname);
	} else for (fd = NULL,i=NINCLUDE-1; i>=0; i--) {
		ip = &includelist[i];
		if (ip->file==NULL || ip->deleted || (angled && ip->always==0))
			continue;
		if (strlen(fname)+strlen(ip->file)+2 > sizeof(iname))
			continue;
		strcpy(iname, ip->file);
		strcat(iname, "/");
		strcat(iname, fname);
		if ((fd = fopen(iname, "r")) != NULL)
			break;
	}
	if ( Mflag>1 || !angled&&Mflag==1 ) {
		fwrite(objname,1,strlen(objname),stdout);
		fwrite(iname,1,strlen(iname),stdout);
		fwrite("\n",1,1,stdout);
	}
	if (fd != NULL) {
		/* check if imported/pragma onced. */
		if (check_hash(iname, type == Import)) {
			fclose(fd);
			return;
		}
		if (++incdepth > 10)
			error(FATAL, "#include too deeply nested");
		setsource((char*)newstring((uchar*)iname, strlen(iname), 0), fd, NULL);
		genline();
	} else {
		trp->tp = trp->bp+2;
		error(ERROR, "Could not find include file %r", trp);
	}
	return;
syntax:
	error(ERROR, "Syntax error in #include");
	return;
}

/*
 * Generate a line directive for cursource
 */
void
genline(void)
{
	static Token ta = { UNCLASS };
	static Tokenrow tr = { &ta, &ta, &ta+1, 1 };
	uchar *p;

	ta.t = p = (uchar*)outp;
	strcpy((char*)p, "#line ");
	p += sizeof("#line ")-1;
	p = (uchar*)outnum((char*)p, cursource->line);
	*p++ = ' '; *p++ = '"';
	strcpy((char*)p, cursource->filename);
	p += strlen((char*)p);
	*p++ = '"'; *p++ = '\n';
	ta.len = (char*)p-outp;
	outp = (char*)p;
	tr.tp = tr.bp;
	puttokens(&tr);
}

void
setobjname(char *f)
{
	int n = strlen(f);
	objname = (char*)domalloc(n+5);
	strcpy(objname,f);
	if(objname[n-2]=='.'){
		strcpy(objname+n-1,"$O: ");
	}else{
		strcpy(objname+n,"$O: ");
	}
}
