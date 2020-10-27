/*
 * Markdown to HTML converter
 */

#include <stdio.h>

int
peak(FILE *fp)
{
	int ch;

	ungetc((ch = fgetc(fp)), fp);
	return ch;
}

int
next(FILE *fp, int want)
{
	int ch;

	ch = fgetc(fp);
	if (ch != want)
		ungetc(ch, fp);
	return ch;
}

void
code(FILE *fp)
{
	int ch;

	printf("<code>");
	while ((ch = fgetc(fp)) != EOF)
		switch (ch) {
		case '`':
			goto end;
		default:
			putchar(ch);
		}
end:
	printf("</code>");
}

void
esccode(FILE *fp)
{
	int ch;

	printf("<code>");
	while ((ch = fgetc(fp)) != EOF)
		switch (ch) {
		case '`':
			if (next(fp, '`') == '`')
				goto end;
		default:
			putchar(ch);
		}
end:
	printf("</code>");
}

void
codeblock(FILE *fp)
{
	int ch;

	printf("<pre><code>");

	/* Skip leading newlines in multi-line codeblock */
	while (next(fp, '\n') == '\n');

	while ((ch = fgetc(fp)) != EOF)
		switch (ch) {
		case '`':
			if (next(fp, '`') == '`' && next(fp, '`') == '`')
				goto end;
		default:
			putchar(ch);
		}
end:
	printf("</code></pre>");
}

void
blockquote(FILE *fp)
{
	int ch;

	printf("<blockquote>");
	while ((ch = fgetc(fp)) != EOF)
		switch (ch) {
		case '\n':
			/* End of blockquote */
			if (next(fp, '\n') == '\n')
				goto end;

			/* Ignore > on subsequent lines */
			if (next(fp, '>') == '>')
				break;
		default:
			putchar(ch);
		}
end:
	printf("</blockquote>");
}

void
list(FILE *fp)
{
	int ch;

	printf("<ul><li>");

	while ((ch = fgetc(fp)) != EOF)
		switch (ch) {
		case '\n':
			if (next(fp, '\n') == '\n')
				goto end;

			if (next(fp, '*') == '*') {
				printf("</li><li>");
				break;
			}
		default:
			putchar(ch);
		}

end:
	printf("</li></ul>");
}

void
paragraph(FILE *fp)
{
	int ch;

	printf("<p>");
	while ((ch = fgetc(fp)) != EOF)
		switch (ch) {
		case '`':
			if (next(fp, '`') == '`') {
				if (next(fp, '`') == '`') { /* Code block */
					/* NOTE: codeblock imples end of p */
					ungetc('`', fp);
					ungetc('`', fp);
					ungetc('`', fp);
					goto end;
				} else {                    /* Escaped code */
					esccode(fp);
				}
			} else {                            /* Inline code */
				code(fp);
			}
			break;
		case '#':
			/* Heading implies end of p */
			ungetc(ch, fp);
			goto end;
		case '\n':
			/* Blockquote */
			if (peak(fp) == '>') {
				ungetc(ch, fp);
				goto end;
			}
			/* End of paragraph */
			if (next(fp, '\n') == '\n')
				goto end;
		default:
			putchar(ch);
		}

	/* End of paragraph */
end:
	printf("</p>");
}

void
heading(FILE *fp, int lvl)
{
	int ch;

	printf("<h%d>", lvl);
	while ((ch = fgetc(fp)) != EOF)
		switch (ch) {
		case '\n':
			goto end;
		default:
			putchar(ch);
		}
end:
	printf("</h%d>", lvl);
}

void
md2html(FILE *fp)
{
	int ch, cnt;

	for (;;)
		switch ((ch = fgetc(fp))) {
		case EOF:
			return;
		/* Consume newlines at the start of top level blocks */
		case '\n':
			break;
		case '>':
			blockquote(fp);
			break;
		case '*':
			list(fp);
			break;
		case '#':
			cnt = 1;
			while (next(fp, '#') == '#')
				++cnt;
			heading(fp, cnt);
			break;
		case '`':
			if (next(fp, '`') == '`' && next(fp, '`') == '`') {
				codeblock(fp);
				break;
			}
		default:
			ungetc(ch, fp);
			paragraph(fp);
		}
}

int
main(int argc, char *argv[])
{
	FILE *fp;

	if (argc < 2) {
		fp = stdin;
	} else {
		if (!(fp = fopen(argv[1], "r"))) {
			perror(argv[1]);
			return 1;
		}
	}

	md2html(fp);
	fclose(fp);
	return 0;
}
