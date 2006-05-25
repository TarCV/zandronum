/* Generated by re2c 0.10.3 on Wed May 24 23:26:39 2006 */
#line 1 "scanner.re"
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "dehsupp.h"

#define	BSIZE	8192

#define	YYCTYPE		uchar
#define	YYCURSOR	cursor
#define	YYLIMIT		s->lim
#define	YYMARKER	s->ptr
#define	YYFILL(n)	{cursor = fill(s, cursor);}

#define	RET(i)	{s->cur = cursor; return i;}

uchar *fill(Scanner *s, uchar *cursor)
{
	if(!s->eof)
	{
		ptrdiff_t cnt = s->tok - s->bot;
		if(cnt)
		{
			memcpy(s->bot, s->tok, s->lim - s->tok);
			s->tok = s->bot;
			s->ptr -= cnt;
			cursor -= cnt;
			s->pos -= cnt;
			s->lim -= cnt;
		}
		if((s->top - s->lim) < BSIZE)
		{
			uchar *buf = (uchar*) malloc(((s->lim - s->bot) + BSIZE)*sizeof(uchar));
			memcpy(buf, s->tok, s->lim - s->tok);
			s->tok = buf;
			s->ptr = &buf[s->ptr - s->bot];
			cursor = &buf[cursor - s->bot];
			s->pos = &buf[s->pos - s->bot];
			s->lim = &buf[s->lim - s->bot];
			s->top = &s->lim[BSIZE];
			free(s->bot);
			s->bot = buf;
		}
		if((cnt = fread((char*) s->lim, 1, BSIZE, s->fd)) != BSIZE)
		{
			s->eof = &s->lim[cnt]; *(s->eof)++ = '\n';
		}
		s->lim += cnt;
	}
	return cursor;
}

int scan(Scanner *s)
{
	uchar *cursor = s->cur;
std:
	s->tok = cursor;
#line 64 "scanner.re"



#line 64 "scanner.c"
{
	YYCTYPE yych;
	unsigned int yyaccept = 0;

	if((YYLIMIT - YYCURSOR) < 13) YYFILL(13);
	yych = *YYCURSOR;
	switch(yych){
	case 0x09:
	case 0x0B:
	case 0x0C:
	case ' ':	goto yy22;
	case 0x0A:	goto yy46;
	case '"':	goto yy20;
	case '&':	goto yy28;
	case '(':	goto yy36;
	case '*':	goto yy34;
	case '+':	goto yy32;
	case ',':	goto yy40;
	case '-':	goto yy30;
	case '/':	goto yy2;
	case '0':	goto yy17;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':	goto yy19;
	case ';':	goto yy44;
	case 'A':	goto yy7;
	case 'B':
	case 'E':
	case 'G':
	case 'H':
	case 'J':
	case 'K':
	case 'L':
	case 'M':
	case 'N':
	case 'P':
	case 'Q':
	case 'U':
	case 'V':
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
	case '_':
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'f':
	case 'g':
	case 'h':
	case 'i':
	case 'j':
	case 'k':
	case 'l':
	case 'm':
	case 'n':
	case 'o':
	case 'q':
	case 'r':
	case 's':
	case 't':
	case 'u':
	case 'v':
	case 'w':
	case 'x':
	case 'y':
	case 'z':	goto yy16;
	case 'C':	goto yy9;
	case 'D':	goto yy13;
	case 'F':	goto yy14;
	case 'I':	goto yy11;
	case 'O':	goto yy8;
	case 'R':	goto yy15;
	case 'S':	goto yy10;
	case 'T':	goto yy12;
	case '^':	goto yy26;
	case 'e':	goto yy4;
	case 'p':	goto yy6;
	case '{':	goto yy42;
	case '|':	goto yy24;
	case '}':	goto yy38;
	default:	goto yy48;
	}
yy2:
	yyaccept = 0;
	yych = *(YYMARKER = ++YYCURSOR);
	if(yych == '*') goto yy198;
	if(yych == '/') goto yy196;
yy3:
#line 107 "scanner.re"
	{ RET(DIVIDE); }
#line 163 "scanner.c"
yy4:
	++YYCURSOR;
	if((yych = *YYCURSOR) == 'n') goto yy192;
	goto yy70;
yy5:
#line 91 "scanner.re"
	{ RET(SYM); }
#line 171 "scanner.c"
yy6:
	yych = *++YYCURSOR;
	if(yych == 'r') goto yy187;
	goto yy70;
yy7:
	yych = *++YYCURSOR;
	if(yych == 'c') goto yy175;
	goto yy70;
yy8:
	yych = *++YYCURSOR;
	if(yych == 'r') goto yy156;
	goto yy70;
yy9:
	yych = *++YYCURSOR;
	if(yych == 'o') goto yy147;
	goto yy70;
yy10:
	yych = *++YYCURSOR;
	if(yych <= 'p') {
		if(yych <= 'n') goto yy70;
		if(yych <= 'o') goto yy121;
		goto yy122;
	} else {
		if(yych == 't') goto yy123;
		goto yy70;
	}
yy11:
	yych = *++YYCURSOR;
	if(yych == 'n') goto yy112;
	goto yy70;
yy12:
	yych = *++YYCURSOR;
	if(yych == 'h') goto yy103;
	goto yy70;
yy13:
	yych = *++YYCURSOR;
	if(yych == 'e') goto yy93;
	goto yy70;
yy14:
	yych = *++YYCURSOR;
	if(yych == 'i') goto yy83;
	goto yy70;
yy15:
	yych = *++YYCURSOR;
	if(yych == 'e') goto yy71;
	goto yy70;
yy16:
	yych = *++YYCURSOR;
	goto yy70;
yy17:
	yyaccept = 1;
	yych = *(YYMARKER = ++YYCURSOR);
	if(yych == 'X') goto yy66;
	if(yych == 'x') goto yy66;
	goto yy65;
yy18:
#line 94 "scanner.re"
	{ RET(NUM); }
#line 230 "scanner.c"
yy19:
	yych = *++YYCURSOR;
	goto yy63;
yy20:
	yyaccept = 2;
	yych = *(YYMARKER = ++YYCURSOR);
	if(yych != 0x0A) goto yy52;
yy21:
#line 124 "scanner.re"
	{
		printf("unexpected character: %c\n", *s->tok);
		goto std;
		}
#line 244 "scanner.c"
yy22:
	++YYCURSOR;
	yych = *YYCURSOR;
	goto yy50;
yy23:
#line 99 "scanner.re"
	{ goto std; }
#line 252 "scanner.c"
yy24:
	++YYCURSOR;
#line 101 "scanner.re"
	{ RET(OR); }
#line 257 "scanner.c"
yy26:
	++YYCURSOR;
#line 102 "scanner.re"
	{ RET(XOR); }
#line 262 "scanner.c"
yy28:
	++YYCURSOR;
#line 103 "scanner.re"
	{ RET(AND); }
#line 267 "scanner.c"
yy30:
	++YYCURSOR;
#line 104 "scanner.re"
	{ RET(MINUS); }
#line 272 "scanner.c"
yy32:
	++YYCURSOR;
#line 105 "scanner.re"
	{ RET(PLUS); }
#line 277 "scanner.c"
yy34:
	++YYCURSOR;
#line 106 "scanner.re"
	{ RET(MULTIPLY); }
#line 282 "scanner.c"
yy36:
	++YYCURSOR;
#line 108 "scanner.re"
	{ RET(LPAREN); }
#line 287 "scanner.c"
yy38:
	++YYCURSOR;
#line 109 "scanner.re"
	{ RET(RPAREN); }
#line 292 "scanner.c"
yy40:
	++YYCURSOR;
#line 110 "scanner.re"
	{ RET(COMMA); }
#line 297 "scanner.c"
yy42:
	++YYCURSOR;
#line 111 "scanner.re"
	{ RET(LBRACE); }
#line 302 "scanner.c"
yy44:
	++YYCURSOR;
#line 113 "scanner.re"
	{ RET(SEMICOLON); }
#line 307 "scanner.c"
yy46:
	++YYCURSOR;
#line 117 "scanner.re"
	{
		if(cursor == s->eof) RET(EOI);
		s->pos = cursor; s->line++;
		goto std;
		}
#line 316 "scanner.c"
yy48:
	yych = *++YYCURSOR;
	goto yy21;
yy49:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
yy50:
	if(yych <= 0x0A) {
		if(yych == 0x09) goto yy49;
		goto yy23;
	} else {
		if(yych <= 0x0C) goto yy49;
		if(yych == ' ') goto yy49;
		goto yy23;
	}
yy51:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
yy52:
	if(yych <= '!') {
		if(yych != 0x0A) goto yy51;
	} else {
		if(yych <= '"') goto yy55;
		if(yych == '\\') goto yy54;
		goto yy51;
	}
yy53:
	YYCURSOR = YYMARKER;
	if(yyaccept <= 1) {
		if(yyaccept <= 0) {
			goto yy3;
		} else {
			goto yy18;
		}
	} else {
		goto yy21;
	}
yy54:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if(yych <= 'b') {
		if(yych <= '7') {
			if(yych <= '&') {
				if(yych == '"') goto yy51;
				goto yy53;
			} else {
				if(yych <= '\'') goto yy51;
				if(yych <= '/') goto yy53;
				goto yy58;
			}
		} else {
			if(yych <= '[') {
				if(yych == '?') goto yy51;
				goto yy53;
			} else {
				if(yych <= '\\') goto yy51;
				if(yych <= '`') goto yy53;
				goto yy51;
			}
		}
	} else {
		if(yych <= 'r') {
			if(yych <= 'm') {
				if(yych == 'f') goto yy51;
				goto yy53;
			} else {
				if(yych <= 'n') goto yy51;
				if(yych <= 'q') goto yy53;
				goto yy51;
			}
		} else {
			if(yych <= 'u') {
				if(yych == 't') goto yy51;
				goto yy53;
			} else {
				if(yych <= 'v') goto yy51;
				if(yych == 'x') goto yy57;
				goto yy53;
			}
		}
	}
yy55:
	++YYCURSOR;
#line 97 "scanner.re"
	{ RET(STRING); }
#line 405 "scanner.c"
yy57:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if(yych <= '@') {
		if(yych <= '/') goto yy53;
		if(yych <= '9') goto yy60;
		goto yy53;
	} else {
		if(yych <= 'F') goto yy60;
		if(yych <= '`') goto yy53;
		if(yych <= 'f') goto yy60;
		goto yy53;
	}
yy58:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if(yych <= '"') {
		if(yych == 0x0A) goto yy53;
		if(yych <= '!') goto yy51;
		goto yy55;
	} else {
		if(yych <= '7') {
			if(yych <= '/') goto yy51;
			goto yy58;
		} else {
			if(yych == '\\') goto yy54;
			goto yy51;
		}
	}
yy60:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if(yych <= '9') {
		if(yych <= '!') {
			if(yych == 0x0A) goto yy53;
			goto yy51;
		} else {
			if(yych <= '"') goto yy55;
			if(yych <= '/') goto yy51;
			goto yy60;
		}
	} else {
		if(yych <= '[') {
			if(yych <= '@') goto yy51;
			if(yych <= 'F') goto yy60;
			goto yy51;
		} else {
			if(yych <= '\\') goto yy54;
			if(yych <= '`') goto yy51;
			if(yych <= 'f') goto yy60;
			goto yy51;
		}
	}
yy62:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
yy63:
	if(yych <= '/') goto yy18;
	if(yych <= '9') goto yy62;
	goto yy18;
yy64:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
yy65:
	if(yych <= '/') goto yy18;
	if(yych <= '9') goto yy64;
	goto yy18;
yy66:
	yych = *++YYCURSOR;
	if(yych <= '@') {
		if(yych <= '/') goto yy53;
		if(yych >= ':') goto yy53;
	} else {
		if(yych <= 'F') goto yy67;
		if(yych <= '`') goto yy53;
		if(yych >= 'g') goto yy53;
	}
yy67:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if(yych <= '@') {
		if(yych <= '/') goto yy18;
		if(yych <= '9') goto yy67;
		goto yy18;
	} else {
		if(yych <= 'F') goto yy67;
		if(yych <= '`') goto yy18;
		if(yych <= 'f') goto yy67;
		goto yy18;
	}
yy69:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
yy70:
	if(yych <= 'Z') {
		if(yych <= '/') goto yy5;
		if(yych <= '9') goto yy69;
		if(yych <= '@') goto yy5;
		goto yy69;
	} else {
		if(yych <= '_') {
			if(yych <= '^') goto yy5;
			goto yy69;
		} else {
			if(yych <= '`') goto yy5;
			if(yych <= 'z') goto yy69;
			goto yy5;
		}
	}
yy71:
	yych = *++YYCURSOR;
	if(yych != 'n') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'd') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'r') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'S') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'y') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'l') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 's') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy82;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy82;
			if(yych <= 'z') goto yy69;
		}
	}
yy82:
#line 89 "scanner.re"
	{ RET(RenderStyles); }
#line 559 "scanner.c"
yy83:
	yych = *++YYCURSOR;
	if(yych != 'r') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 's') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'S') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy92;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy92;
			if(yych <= 'z') goto yy69;
		}
	}
yy92:
#line 88 "scanner.re"
	{ RET(FirstState); }
#line 593 "scanner.c"
yy93:
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'h') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'S') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy102;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy102;
			if(yych <= 'z') goto yy69;
		}
	}
yy102:
#line 86 "scanner.re"
	{ RET(DeathState); }
#line 627 "scanner.c"
yy103:
	yych = *++YYCURSOR;
	if(yych != 'i') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'n') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'g') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'B') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'i') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 's') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy111;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy111;
			if(yych <= 'z') goto yy69;
		}
	}
yy111:
#line 85 "scanner.re"
	{ RET(ThingBits); }
#line 659 "scanner.c"
yy112:
	yych = *++YYCURSOR;
	if(yych != 'f') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'o') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'N') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'm') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 's') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy120;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy120;
			if(yych <= 'z') goto yy69;
		}
	}
yy120:
#line 84 "scanner.re"
	{ RET(InfoNames); }
#line 691 "scanner.c"
yy121:
	yych = *++YYCURSOR;
	if(yych == 'u') goto yy140;
	goto yy70;
yy122:
	yych = *++YYCURSOR;
	if(yych == 'a') goto yy131;
	goto yy70;
yy123:
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'M') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'p') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy130;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy130;
			if(yych <= 'z') goto yy69;
		}
	}
yy130:
#line 82 "scanner.re"
	{ RET(StateMap); }
#line 729 "scanner.c"
yy131:
	yych = *++YYCURSOR;
	if(yych != 'w') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'n') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'S') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy139;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy139;
			if(yych <= 'z') goto yy69;
		}
	}
yy139:
#line 87 "scanner.re"
	{ RET(SpawnState); }
#line 761 "scanner.c"
yy140:
	yych = *++YYCURSOR;
	if(yych != 'n') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'd') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'M') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'p') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy146;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy146;
			if(yych <= 'z') goto yy69;
		}
	}
yy146:
#line 83 "scanner.re"
	{ RET(SoundMap); }
#line 789 "scanner.c"
yy147:
	yych = *++YYCURSOR;
	if(yych != 'd') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'P') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'C') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'o') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'n') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'v') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy155;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy155;
			if(yych <= 'z') goto yy69;
		}
	}
yy155:
#line 80 "scanner.re"
	{ RET(CodePConv); }
#line 821 "scanner.c"
yy156:
	yych = *++YYCURSOR;
	if(yych != 'g') goto yy70;
	yych = *++YYCURSOR;
	if(yych == 'H') goto yy158;
	if(yych == 'S') goto yy159;
	goto yy70;
yy158:
	yych = *++YYCURSOR;
	if(yych == 'e') goto yy168;
	goto yy70;
yy159:
	yych = *++YYCURSOR;
	if(yych != 'p') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'r') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'N') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'a') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'm') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'e') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 's') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy167;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy167;
			if(yych <= 'z') goto yy69;
		}
	}
yy167:
#line 81 "scanner.re"
	{ RET(OrgSprNames); }
#line 864 "scanner.c"
yy168:
	yych = *++YYCURSOR;
	if(yych != 'i') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'g') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'h') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 's') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy174;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy174;
			if(yych <= 'z') goto yy69;
		}
	}
yy174:
#line 78 "scanner.re"
	{ RET(OrgHeights); }
#line 892 "scanner.c"
yy175:
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'i') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'o') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'n') goto yy70;
	yych = *++YYCURSOR;
	if(yych == 'L') goto yy182;
	if(yych != 's') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy181;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy181;
			if(yych <= 'z') goto yy69;
		}
	}
yy181:
#line 77 "scanner.re"
	{ RET(Actions); }
#line 921 "scanner.c"
yy182:
	yych = *++YYCURSOR;
	if(yych != 'i') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 's') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy186;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy186;
			if(yych <= 'z') goto yy69;
		}
	}
yy186:
#line 79 "scanner.re"
	{ RET(ActionList); }
#line 945 "scanner.c"
yy187:
	yych = *++YYCURSOR;
	if(yych != 'i') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'n') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 't') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy191;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy191;
			if(yych <= 'z') goto yy69;
		}
	}
yy191:
#line 76 "scanner.re"
	{ RET(PRINT); }
#line 969 "scanner.c"
yy192:
	yych = *++YYCURSOR;
	if(yych != 'd') goto yy70;
	yych = *++YYCURSOR;
	if(yych != 'l') goto yy70;
	++YYCURSOR;
	if((yych = *YYCURSOR) <= 'Z') {
		if(yych <= '/') goto yy195;
		if(yych <= '9') goto yy69;
		if(yych >= 'A') goto yy69;
	} else {
		if(yych <= '_') {
			if(yych >= '_') goto yy69;
		} else {
			if(yych <= '`') goto yy195;
			if(yych <= 'z') goto yy69;
		}
	}
yy195:
#line 75 "scanner.re"
	{ RET(ENDL); }
#line 991 "scanner.c"
yy196:
	++YYCURSOR;
	if(YYLIMIT == YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if(yych == 0x0A) goto yy200;
	goto yy196;
yy198:
	++YYCURSOR;
#line 67 "scanner.re"
	{ goto comment; }
#line 1002 "scanner.c"
yy200:
	++YYCURSOR;
#line 69 "scanner.re"
	{
		if(cursor == s->eof) RET(EOI);
		s->tok = s->pos = cursor; s->line++;
		goto std;
		}
#line 1011 "scanner.c"
}
#line 128 "scanner.re"


comment:

#line 1018 "scanner.c"
{
	YYCTYPE yych;
	if((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	if(yych == 0x0A) goto yy206;
	if(yych != '*') goto yy208;
	++YYCURSOR;
	if((yych = *YYCURSOR) == '/') goto yy209;
yy205:
#line 139 "scanner.re"
	{ goto comment; }
#line 1030 "scanner.c"
yy206:
	++YYCURSOR;
#line 134 "scanner.re"
	{
		if(cursor == s->eof) RET(EOI);
		s->tok = s->pos = cursor; s->line++;
		goto comment;
		}
#line 1039 "scanner.c"
yy208:
	yych = *++YYCURSOR;
	goto yy205;
yy209:
	++YYCURSOR;
#line 132 "scanner.re"
	{ goto std; }
#line 1047 "scanner.c"
}
#line 140 "scanner.re"

}

int lex(Scanner *s, struct Token *tok)
{
	int tokentype = scan(s);
	char *p, *q;

	tok->val = 0;
	tok->string = NULL;

	switch (tokentype)
	{
	case NUM:
		tok->val = strtol((char *)s->tok, NULL, 0);
		break;

	case STRING:
		tok->string = (char *)malloc(s->cur - s->tok - 1);
		strncpy(tok->string, (char *)s->tok + 1, s->cur - s->tok - 2);
		tok->string[s->cur - s->tok - 2] = '\0';
		for (p = q = tok->string; *p; ++p, ++q)
		{
			if (p[0] == '\\' && p[1] == '\\')
				++p;
			*q = *p;
		}
		break;

	case SYM:
		tok->string = (char *)malloc(s->cur - s->tok + 1);
		strncpy(tok->string, (char *)s->tok, s->cur - s->tok);
		tok->string[s->cur - s->tok] = '\0';
		break;
	}
	return tokentype;
}
