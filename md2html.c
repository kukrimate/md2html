/*
 * Markdown to HTML converter
 */

#include <stdio.h>

//
// HTML 13.3 Serializing HTML fragments - "Escaping a string"
// NOTE: we don't convert U+00A0 to &nbsp; to avoid having to parse UTF-8.
// URL: https://html.spec.whatwg.org/#serialising-html-fragments
//
static void put_escaped(int ch)
{
    switch (ch) {
    case '&':   fputs("&amp;", stdout);     break;
    case '"':   fputs("&quot;", stdout);    break;
    case '<':   fputs("&lt;", stdout);      break;
    case '>':   fputs("&gt;", stdout);      break;
    default:    putchar(ch);                break;
    }
}

static void rawhtml(FILE *fp)
{
    int ch;
    for (;;)
        switch ((ch = fgetc(fp))) {
        case '\\':
            putchar(fgetc(fp));
            break;
        case '$':
            return;
        default:
            putchar(ch);
        }
}

static int peek(FILE *fp)
{
    int ch;

    ungetc((ch = fgetc(fp)), fp);
    return ch;
}

static _Bool next(FILE *fp, int want)
{
    int ch;

    ch = fgetc(fp);
    if (ch != want) {
        ungetc(ch, fp);
        return 0;
    }
    return 1;
}

static _Bool nextstr(FILE *fp, char *want)
{
    char *cur;
    int ch;

    for (cur = want; *cur; ++cur)
        if ((ch = fgetc(fp)) != *cur)
            goto fail;
    return 1;
fail:
    ungetc(ch, fp);
    while (--cur >= want)
        ungetc(*cur, fp);
    return 0;
}

static void skip_white(FILE *fp)
{
    int ch;

    while ((ch = fgetc(fp)) != EOF)
        switch (ch) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
        case '\v':
        case '\f':
            break;
        default:
            ungetc(ch, fp);
            return;
        }
}

static void code(FILE *fp)
{
    int ch;

    printf("<code>");
    while ((ch = fgetc(fp)) != EOF)
        switch (ch) {
        case '`':
            goto end;
        default:
            put_escaped(ch);
        }
end:
    printf("</code>");
}

static void esccode(FILE *fp)
{
    int ch;

    printf("<code>");
    while ((ch = fgetc(fp)) != EOF)
        switch (ch) {
        case '`':
            if (next(fp, '`'))
                goto end;
        default:
            put_escaped(ch);
        }
end:
    printf("</code>");
}

static void codeblock(FILE *fp)
{
    int ch;

    skip_white(fp);
    printf("<pre><code>");
    while ((ch = fgetc(fp)) != EOF)
        switch (ch) {
        case '`':
            if (nextstr(fp, "``"))
                goto end;
        default:
            put_escaped(ch);
        }
end:
    printf("</code></pre>\n");
}

static void heading(FILE *fp, int lvl)
{
    int ch;

    skip_white(fp);
    printf("<h%d>", lvl);
    while ((ch = fgetc(fp)) != EOF)
        switch (ch) {
        /* End of heading */
        case '\n':
            goto end;
        default:
            put_escaped(ch);
        }
end:
    printf("</h%d>\n", lvl);
}

static void blockquote(FILE *fp)
{
    int ch;

    printf("<blockquote>");
    while ((ch = fgetc(fp)) != EOF)
        switch (ch) {
        case '\n':
            /* End of blockquote */
            if (next(fp, '\n'))
                goto end;

            /* Next top level element */
            switch (peek(fp)) {
            case '#': /* Heading */
            case '*': /* List */
                goto end;
            case '`': /* Code block */
                if (nextstr(fp, "```")) {
                    ungetc('`', fp);
                    ungetc('`', fp);
                    ungetc('`', fp);
                    goto end;
                }
            }

            /* Add break if > appears on subsequent lines */
            if (next(fp, '>')) {
                printf("<br/>");
                break;
            }
        default:
            put_escaped(ch);
        }
end:
    printf("</blockquote>\n");
}

static void list(FILE *fp)
{
    int ch;

    printf("<ul><li>");

    while ((ch = fgetc(fp)) != EOF)
        switch (ch) {
        case '\\':
            putchar(fgetc(fp));
            break;
        case '`':
            if (next(fp, '`'))   /* Escaped code */
                esccode(fp);
            else                 /* Normal code */
                code(fp);
            break;
        case '$':
            rawhtml(fp);
            break;
        case '*':
            if (!next(fp, '*')) {
                putchar('*');
                continue;
            }
            printf("<b>");
            while (!nextstr(fp, "**"))
                putchar(fgetc(fp));
            printf("</b>");
            break;
        case '_':
            printf("<it>");
            while (!next(fp, '_'))
                putchar(fgetc(fp));
            printf("</it>");
            break;
        case '\n':
            /* End of list */
            if (next(fp, '\n'))
                goto end;

            /* Next top level element */
            switch (peek(fp)) {
            case '#': /* Heading */
            case '>': /* Blockquote */
                goto end;
            case '`': /* Code block */
                if (nextstr(fp, "```")) {
                    ungetc('`', fp);
                    ungetc('`', fp);
                    ungetc('`', fp);
                    goto end;
                }
            }

            /* Line starting with * means next element */
            if (next(fp, '*') || next(fp, '-')) {
                printf("</li><li>");
                break;
            }
        default:
            put_escaped(ch);
        }

end:
    printf("</li></ul>\n");
}

static void paragraph(FILE *fp)
{
    int ch;

    printf("<p>");
    while ((ch = fgetc(fp)) != EOF)
        switch (ch) {
        case '\\':
            putchar(fgetc(fp));
            break;
        case '`':
            if (next(fp, '`'))   /* Escaped code */
                esccode(fp);
            else                 /* Normal code */
                code(fp);
            break;
        case '$':
            rawhtml(fp);
            break;
        case '*':
            if (!next(fp, '*')) {
                putchar('*');
                continue;
            }
            printf("<b>");
            while (!nextstr(fp, "**"))
                putchar(fgetc(fp));
            printf("</b>");
            break;
        case '_':
            printf("<it>");
            while (!next(fp, '_'))
                putchar(fgetc(fp));
            printf("</it>");
            break;
        case '\n':
            /* End of paragraph */
            if (next(fp, '\n'))
                goto end;

            /* Next top level element */
            switch (peek(fp)) {
            case '#': /* Heading */
            case '>': /* Blockquote */
            case '*': /* List */
            case '-':
                goto end;
            case '`': /* Code block */
                if (nextstr(fp, "```")) {
                    ungetc('`', fp);
                    ungetc('`', fp);
                    ungetc('`', fp);
                    goto end;
                }
            }
        default:
            put_escaped(ch);
        }

end:
    printf("</p>\n");
}

static void md2html(FILE *fp)
{
    int ch, cnt;

    for (;;)
        switch ((ch = fgetc(fp))) {
        case EOF:
            return;
        /* Consume newlines at the start of top level blocks */
        case '\n':
            break;
        case '#':
            cnt = 1;
            while (next(fp, '#'))
                ++cnt;
            heading(fp, cnt);
            break;
        case '>':
            blockquote(fp);
            break;
        case '*':
        case '-':
            list(fp);
            break;
        case '$':
            rawhtml(fp);
            break;
        case '`':
            if (nextstr(fp, "``")) {
                codeblock(fp);
                break;
            }
        default:
            ungetc(ch, fp);
            paragraph(fp);
        }
}

int main(int argc, char *argv[])
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
