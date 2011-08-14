/*******************************************************

   CoolReader Engine C-compatible API

   lvtextfm.cpp:  Text formatter

   (c) Vadim Lopatin, 2000-2011
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/crsetup.h"
#include "../include/lvfnt.h"
#include "../include/lvtextfm.h"
#include "../include/lvdrawbuf.h"

#ifdef __cplusplus
#include "../include/lvimg.h"
#include "../include/lvtinydom.h"
#endif

// to debug formatter
#if 0
#define TR(x...) CRLog::trace(x)
#else
#ifdef _MSC_VER
#define TR(x)
#else
#define TR(x...)
#endif
#endif

#define FRM_ALLOC_SIZE 16

formatted_line_t * lvtextAllocFormattedLine( )
{
    formatted_line_t * pline = (formatted_line_t *)malloc(sizeof(formatted_line_t));
    memset( pline, 0, sizeof(formatted_line_t) );
    return pline;
}

formatted_line_t * lvtextAllocFormattedLineCopy( formatted_word_t * words, int word_count )
{
    formatted_line_t * pline = (formatted_line_t *)malloc(sizeof(formatted_line_t));
    memset( pline, 0, sizeof(formatted_line_t) );
    lUInt32 size = (word_count + FRM_ALLOC_SIZE-1) / FRM_ALLOC_SIZE * FRM_ALLOC_SIZE;
    pline->words = (formatted_word_t*)malloc( sizeof(formatted_word_t)*(size) );
    memcpy( pline->words, words, word_count * sizeof(formatted_word_t) );
    return pline;
}

void lvtextFreeFormattedLine( formatted_line_t * pline )
{
    if (pline->words)
        free( pline->words );
    free(pline);
}

formatted_word_t * lvtextAddFormattedWord( formatted_line_t * pline )
{
    lUInt32 size = (pline->word_count + FRM_ALLOC_SIZE-1) / FRM_ALLOC_SIZE * FRM_ALLOC_SIZE;
    if ( pline->word_count >= size)
    {
        size += FRM_ALLOC_SIZE;
        pline->words = (formatted_word_t*)realloc( pline->words, sizeof(formatted_word_t)*(size) );
    }
    return &pline->words[ pline->word_count++ ];
}

formatted_line_t * lvtextAddFormattedLine( formatted_text_fragment_t * pbuffer )
{
    lUInt32 size = (pbuffer->frmlinecount + FRM_ALLOC_SIZE-1) / FRM_ALLOC_SIZE * FRM_ALLOC_SIZE;
    if ( pbuffer->frmlinecount >= size)
    {
        size += FRM_ALLOC_SIZE;
        pbuffer->frmlines = (formatted_line_t**)realloc( pbuffer->frmlines, sizeof(formatted_line_t*)*(size) );
    }
    return (pbuffer->frmlines[ pbuffer->frmlinecount++ ] = lvtextAllocFormattedLine());
}

formatted_line_t * lvtextAddFormattedLineCopy( formatted_text_fragment_t * pbuffer, formatted_word_t * words, int words_count )
{
    lUInt32 size = (pbuffer->frmlinecount + FRM_ALLOC_SIZE-1) / FRM_ALLOC_SIZE * FRM_ALLOC_SIZE;
    if ( pbuffer->frmlinecount >= size)
    {
        size += FRM_ALLOC_SIZE;
        pbuffer->frmlines = (formatted_line_t**)realloc( pbuffer->frmlines, sizeof(formatted_line_t*)*(size) );
    }
    return (pbuffer->frmlines[ pbuffer->frmlinecount++ ] = lvtextAllocFormattedLineCopy(words, words_count));
}

formatted_text_fragment_t * lvtextAllocFormatter( lUInt16 width )
{
    formatted_text_fragment_t * pbuffer = (formatted_text_fragment_t*)malloc( sizeof(formatted_text_fragment_t) );
    memset( pbuffer, 0, sizeof(formatted_text_fragment_t));
    pbuffer->width = width;
    int defMode = MAX_IMAGE_SCALE_MUL > 1 ? (ARBITRARY_IMAGE_SCALE_ENABLED==1 ? 2 : 1) : 0;
    int defMult = MAX_IMAGE_SCALE_MUL;
    pbuffer->img_zoom_in_mode_block = defMode; /**< can zoom in block images: 0=disabled, 1=integer scale, 2=free scale */
    pbuffer->img_zoom_in_scale_block = defMult; /**< max scale for block images zoom in: 1, 2, 3 */
    pbuffer->img_zoom_in_mode_inline = defMode; /**< can zoom in inline images: 0=disabled, 1=integer scale, 2=free scale */
    pbuffer->img_zoom_in_scale_inline = defMult; /**< max scale for inline images zoom in: 1, 2, 3 */
    pbuffer->img_zoom_out_mode_block = defMode; /**< can zoom out block images: 0=disabled, 1=integer scale, 2=free scale */
    pbuffer->img_zoom_out_scale_block = defMult; /**< max scale for block images zoom out: 1, 2, 3 */
    pbuffer->img_zoom_out_mode_inline = defMode; /**< can zoom out inline images: 0=disabled, 1=integer scale, 2=free scale */
    pbuffer->img_zoom_out_scale_inline = defMult; /**< max scale for inline images zoom out: 1, 2, 3 */
    return pbuffer;
}

void lvtextFreeFormatter( formatted_text_fragment_t * pbuffer )
{
    if (pbuffer->srctext)
    {
        for (lUInt32 i=0; i<pbuffer->srctextlen; i++)
        {
            if (pbuffer->srctext[i].flags & LTEXT_FLAG_OWNTEXT)
                free( (void*)pbuffer->srctext[i].t.text );
        }
        free( pbuffer->srctext );
    }
    if (pbuffer->frmlines)
    {
        for (lUInt32 i=0; i<pbuffer->frmlinecount; i++)
        {
            lvtextFreeFormattedLine( pbuffer->frmlines[i] );
        }
        free( pbuffer->frmlines );
    }
    free(pbuffer);
}


void lvtextAddSourceLine( formatted_text_fragment_t * pbuffer,
   lvfont_handle   font,     /* handle of font to draw string */
   const lChar16 * text,     /* pointer to unicode text string */
   lUInt32         len,      /* number of chars in text, 0 for auto(strlen) */
   lUInt32         color,    /* color */
   lUInt32         bgcolor,  /* bgcolor */
   lUInt32         flags,    /* flags */
   lUInt8          interval, /* interline space, *16 (16=single, 32=double) */
   lUInt16         margin,   /* first line margin */
   void *          object,    /* pointer to custom object */
   lUInt16         offset,
   lInt8           letter_spacing
                         )
{
    lUInt32 srctextsize = (pbuffer->srctextlen + FRM_ALLOC_SIZE-1) / FRM_ALLOC_SIZE * FRM_ALLOC_SIZE;
    if ( pbuffer->srctextlen >= srctextsize)
    {
        srctextsize += FRM_ALLOC_SIZE;
        pbuffer->srctext = (src_text_fragment_t*)realloc( pbuffer->srctext, sizeof(src_text_fragment_t)*(srctextsize) );
    }
    src_text_fragment_t * pline = &pbuffer->srctext[ pbuffer->srctextlen++ ];
    pline->t.font = font;
    if (!len) for (len=0; text[len]; len++) ;
    if (flags & LTEXT_FLAG_OWNTEXT)
    {
        /* make own copy of text */
        pline->t.text = (lChar16*)malloc( len * sizeof(lChar16) );
        memcpy((void*)pline->t.text, text, len * sizeof(lChar16));
    }
    else
    {
        pline->t.text = text;
    }
    pline->index = (lUInt16)(pbuffer->srctextlen-1);
    pline->object = object;
    pline->t.len = (lUInt16)len;
    pline->margin = margin;
    pline->flags = flags;
    pline->interval = interval;
    pline->t.offset = offset;
    pline->color = color;
    pline->bgcolor = bgcolor;
    pline->letter_spacing = letter_spacing;
}

void lvtextAddSourceObject(
   formatted_text_fragment_t * pbuffer,
   lUInt16         width,
   lUInt16         height,
   lUInt32         flags,    /* flags */
   lUInt8          interval, /* interline space, *16 (16=single, 32=double) */
   lUInt16         margin,   /* first line margin */
   void *          object,    /* pointer to custom object */
   lInt8           letter_spacing
                         )
{
    lUInt32 srctextsize = (pbuffer->srctextlen + FRM_ALLOC_SIZE-1) / FRM_ALLOC_SIZE * FRM_ALLOC_SIZE;
    if ( pbuffer->srctextlen >= srctextsize)
    {
        srctextsize += FRM_ALLOC_SIZE;
        pbuffer->srctext = (src_text_fragment_t*)realloc( pbuffer->srctext, sizeof(src_text_fragment_t)*(srctextsize) );
    }
    src_text_fragment_t * pline = &pbuffer->srctext[ pbuffer->srctextlen++ ];
    pline->index = (lUInt16)(pbuffer->srctextlen-1);
    pline->o.width = width;
    pline->o.height = height;
    pline->object = object;
    pline->margin = margin;
    pline->flags = flags | LTEXT_SRC_IS_OBJECT;
    pline->interval = interval;
    pline->letter_spacing = letter_spacing;
}

int lvtextFinalizeLine( formatted_line_t * frmline, int width, int align,
        lUInt16 * pSrcIndex, lUInt16 * pSrcOffset )
{
    int margin = frmline->x;
    int delta = 0;
    unsigned int i;
    unsigned short w = 0;
    int expand_count = 0;
    int expand_dx, expand_dd;
    int flgRollback = 0;

    if (pSrcIndex!=NULL)
    {
        /* check whether words rollback is necessary */
        for (i=frmline->word_count-1; i>0; i--)
        {
            if ( (frmline->words[i].flags & LTEXT_WORD_CAN_BREAK_LINE_AFTER) )
                break;
            if ( (frmline->words[i].flags & LTEXT_WORD_CAN_HYPH_BREAK_LINE_AFTER) )
                break;
        }
        if (/*i > 0 && */i < frmline->word_count-1)
        {
            /* rollback */
            *pSrcIndex = frmline->words[i+1].src_text_index;
            *pSrcOffset = frmline->words[i+1].t.start;
            frmline->word_count = i+1;
            flgRollback = 1;
        }
    }


    frmline->width = 0;
    for (i=0; i<frmline->word_count; i++)
    {
        //if (i == frmline->word_count-1)
        //    w = frmline->words[i].x;
        //else
        w = frmline->words[i].width;
        frmline->words[i].x = frmline->width;
        frmline->width += w;
    }

    if (align == LTEXT_ALIGN_LEFT)
        return flgRollback;

    delta = width - frmline->width - margin;

    if (align == LTEXT_ALIGN_CENTER)
        delta /= 2;
    if ( align == LTEXT_ALIGN_CENTER || align == LTEXT_ALIGN_RIGHT )
    {
        frmline->x += delta;
    }
    else
    {
        /* LTEXT_ALIGN_WIDTH */
        for (i=0; i<frmline->word_count-1; i++)
        {
            if (frmline->words[i].flags & LTEXT_WORD_CAN_ADD_SPACE_AFTER)
                expand_count++;
        }
        if (expand_count)
        {
            expand_dx = delta / expand_count;
            expand_dd = delta % expand_count;
            delta = 0;
            for (i=0; i<frmline->word_count-1; i++)
            {
                if (frmline->words[i].flags & LTEXT_WORD_CAN_ADD_SPACE_AFTER)
                {
                    delta += expand_dx;
                    if (expand_dd>0)
                    {
                        delta++;
                        expand_dd--;
                    }
                }
                if ( i<frmline->word_count-1 ) {
                    frmline->words[i+1].x += delta;
                }
            }
            frmline->width = frmline->words[frmline->word_count-1].x + frmline->words[frmline->word_count-1].width;
        }
    }
    return flgRollback;
}

#define DEPRECATED_LINE_BREAK_WORD_COUNT    3
#define DEPRECATED_LINE_BREAK_SPACE_LIMIT   64


#ifdef __cplusplus

#define DUMMY_IMAGE_SIZE 16

bool gFlgFloatingPunctuationEnabled = true;

void LFormattedText::AddSourceObject(
            lUInt16         flags,    /* flags */
            lUInt8          interval, /* interline space, *16 (16=single, 32=double) */
            lUInt16         margin,   /* first line margin */
            void *          object,    /* pointer to custom object */
            lInt8           letter_spacing
     )
{
    ldomNode * node = (ldomNode*)object;
    LVImageSourceRef img = node->getObjectImageSource();
    if ( img.isNull() )
        img = LVCreateDummyImageSource( node, DUMMY_IMAGE_SIZE, DUMMY_IMAGE_SIZE );
    lUInt16 width = (lUInt16)img->GetWidth();
    lUInt16 height = (lUInt16)img->GetHeight();
    lvtextAddSourceObject(m_pbuffer,
        width, height,
        flags, interval, margin, object, letter_spacing );
}

class LVFormatter {
public:
    //LVArray<lUInt16>  widths_buf;
    //LVArray<lUInt8>   flags_buf;
    formatted_text_fragment_t * m_pbuffer;
    int       m_length;
    int       m_size;
    bool      m_staticBufs;
    lChar16 * m_text;
    lUInt8 *  m_flags;
    src_text_fragment_t * * m_srcs;
    lUInt16 * m_charindex;
    int *     m_widths;
    int m_y;

#define OBJECT_CHAR_INDEX ((lUInt16)0xFFFF)

    LVFormatter(formatted_text_fragment_t * pbuffer)
    : m_pbuffer(pbuffer), m_length(0), m_size(0), m_staticBufs(true), m_y(0)
    {
        m_text = NULL;
        m_flags = NULL;
        m_srcs = NULL;
        m_charindex = NULL;
        m_widths = NULL;
    }

    ~LVFormatter()
    {
    }

    /// allocate buffers for paragraph
    void allocate( int start, int end )
    {
        int pos = 0;
        unsigned i;
        // PASS 1: calculate total length (characters + objects)
        for ( i=start; i<end; i++ ) {
            src_text_fragment_t * src = &m_pbuffer->srctext[i];
            if ( src->flags & LTEXT_SRC_IS_OBJECT ) {
                pos++;
            } else {
                pos += src->t.len;
            }
        }

        // allocate buffers
        m_length = pos;

        TR("allocate(%d)", m_length);

#define STATIC_BUFS_SIZE 8192
#define ITEMS_RESERVED 16
        if ( !m_staticBufs || m_length>STATIC_BUFS_SIZE-1 ) {
            if ( m_length+ITEMS_RESERVED>m_size ) {
                // realloc
                m_size = m_length+ITEMS_RESERVED;
                m_text = (lChar16*)realloc(m_text, sizeof(lChar16)*m_size);
                m_flags = (lUInt8*)realloc(m_flags, sizeof(lUInt8)*m_size);
                m_charindex = (lUInt16*)realloc(m_charindex, sizeof(lUInt16)*m_size);
                m_srcs = (src_text_fragment_t **)realloc(m_srcs, sizeof(src_text_fragment_t *)*m_size);
                m_widths = (int*)realloc(m_widths, sizeof(int)*m_size);
            }
            m_staticBufs = false;
        } else {
            // static buffer space
            static lChar16 m_static_text[STATIC_BUFS_SIZE];
            static lUInt8 m_static_flags[STATIC_BUFS_SIZE];
            static src_text_fragment_t * m_static_srcs[STATIC_BUFS_SIZE];
            static lUInt16 m_static_charindex[STATIC_BUFS_SIZE];
            static int m_static_widths[STATIC_BUFS_SIZE];
            m_text = m_static_text;
            m_flags = m_static_flags;
            m_charindex = m_static_charindex;
            m_srcs = m_static_srcs;
            m_widths = m_static_widths;
            m_staticBufs = true;
        }
        memset( m_flags, 0, sizeof(lUInt8)*m_length );
        pos = 0;
    }

    /// copy text of current paragraph to buffers
    void copyText( int start, int end )
    {
        int pos = 0;
        unsigned i;
        for ( i=start; i<end; i++ ) {
            src_text_fragment_t * src = &m_pbuffer->srctext[i];
            if ( src->flags & LTEXT_SRC_IS_OBJECT ) {
                m_text[pos] = 0;
                m_flags[pos] = LCHAR_IS_OBJECT | LCHAR_ALLOW_WRAP_AFTER;
                m_srcs[pos] = src;
                m_charindex[pos] = OBJECT_CHAR_INDEX; //0xFFFF;
                pos++;
            } else {
                int len = src->t.len;
                lStr_ncpy( m_text+pos, src->t.text, len );
                if ( i==0 || (src->flags & LTEXT_FLAG_NEWLINE) )
                    m_flags[pos] = LCHAR_MANDATORY_NEWLINE;
                for ( int k=0; k<len; k++ ) {
                    m_charindex[pos] = k;
                    m_srcs[pos] = src;
                    lChar16 ch = m_text[pos];
                    if ( ch == '-' || ch == 0x2010 || ch == '.' || ch == '+' )
                        m_flags[pos] |= LCHAR_DEPRECATED_WRAP_AFTER;
                    pos++;
                }
            }
        }
        TR("%s", LCSTR(lString16(m_text, m_length)));
    }

    void resizeImage( int & width, int & height, int maxw, int maxh, bool isInline )
    {
        bool arbitraryImageScaling = false;
        int maxScale = 1;
        bool zoomIn = width<maxw && height<maxh;
        if ( isInline ) {
            if ( zoomIn ) {
                if ( m_pbuffer->img_zoom_in_mode_inline==0 )
                    return; // no zoom
                arbitraryImageScaling = m_pbuffer->img_zoom_in_mode_inline == 2;
                maxScale = m_pbuffer->img_zoom_in_scale_inline;
            } else {
//                if ( m_pbuffer->img_zoom_out_mode_inline==0 )
//                    return; // no zoom
                arbitraryImageScaling = m_pbuffer->img_zoom_out_mode_inline == 2;
                maxScale = m_pbuffer->img_zoom_out_scale_inline;
            }
        } else {
            if ( zoomIn ) {
                if ( m_pbuffer->img_zoom_in_mode_block==0 )
                    return; // no zoom
                arbitraryImageScaling = m_pbuffer->img_zoom_in_mode_block == 2;
                maxScale = m_pbuffer->img_zoom_in_scale_block;
            } else {
//                if ( m_pbuffer->img_zoom_out_mode_block==0 )
//                    return; // no zoom
                arbitraryImageScaling = m_pbuffer->img_zoom_out_mode_block == 2;
                maxScale = m_pbuffer->img_zoom_out_scale_block;
            }
        }
        resizeImage( width, height, maxw, maxh, arbitraryImageScaling, maxScale );
    }

    void resizeImage( int & width, int & height, int maxw, int maxh, bool arbitraryImageScaling, int maxScaleMult )
    {
        if ( maxScaleMult<1 )
            maxScaleMult = 1;
        if ( arbitraryImageScaling ) {
            int pscale_x = 1000 * maxw / width;
            int pscale_y = 1000 * maxh / height;
            int pscale = pscale_x < pscale_y ? pscale_x : pscale_y;
            int maxscale = (MAX_IMAGE_SCALE_MUL>0 ? MAX_IMAGE_SCALE_MUL : 1) * 1000;
            if ( pscale>maxscale )
                pscale = maxscale;
            height = height * pscale / 1000;
            width = width * pscale / 1000;
        } else {
            int scale_div = 1;
            int scale_mul = 1;
            int div_x = (width / maxw) + 1;
            int div_y = (height / maxh) + 1;
            if ( maxScaleMult>=3 && height*3 < maxh - 20
                    && width*3 < maxw - 20 ) {
                scale_mul = 3;
            } else if ( maxScaleMult>=2 && height * 2 < maxh - 20
                    && width * 2 < maxw - 20 ) {
                scale_mul = 2;
            } else if (div_x>1 || div_y>1) {
                if (div_x>div_y)
                    scale_div = div_x;
                else
                    scale_div = div_y;
            }
            height = height * scale_mul / scale_div;
            width = width * scale_mul / scale_div;
        }
    }

    /// measure text of current paragraph
    void measureText()
    {
        int i;
        LVFont * lastFont = NULL;
        src_text_fragment_t * lastSrc = NULL;
        int start = 0;
        int lastWidth = 0;
#define MAX_TEXT_CHUNK_SIZE 4096
        static lUInt16 widths[MAX_TEXT_CHUNK_SIZE+1];
        static lUInt8 flags[MAX_TEXT_CHUNK_SIZE+1];
        int tabIndex = -1;
        for ( i=0; i<=m_length; i++ ) {
            LVFont * newFont = NULL;
            src_text_fragment_t * newSrc = NULL;
            if ( tabIndex<0 && m_text[i]=='\t' ) {
                tabIndex = i;
            }
            bool isObject = false;
            if ( i<m_length ) {
                newSrc = m_srcs[i];
                newFont = (LVFont *)newSrc->t.font;
                isObject = m_charindex[i]==OBJECT_CHAR_INDEX;
            }
            if ( !lastFont )
                lastFont = newFont;
            if ( i>start && (newFont!=lastFont || isObject || i>=start+MAX_TEXT_CHUNK_SIZE || (m_flags[i]&LCHAR_MANDATORY_NEWLINE)) ) {
                // measure start..i-1 chars
                if ( m_charindex[i-1]!=OBJECT_CHAR_INDEX ) {
                    // measure text
                    int len = i - start;
                    int chars_measured = lastFont->measureText(
                            m_text + start,
                            len,
                            widths, flags,
                            0x7FFF, //pbuffer->width,
                            //300, //TODO
                            '?',
                            m_srcs[start]->letter_spacing,
                            false);
                    if ( chars_measured<len ) {
                        // too long line
                        int newlen = chars_measured; // TODO: find best wrap position
                        i = start + newlen;
                        len = newlen;
                    }

                    for ( int k=0; k<len; k++ ) {
                        m_widths[start + k] = lastWidth + widths[k];
                        m_flags[start + k] |= flags[k];
                    }

//                    // debug dump
//                    lString16 buf;
//                    for ( int k=0; k<len; k++ ) {
//                        buf << L"_" << lChar16(m_text[start+k]) << L"_" << lString16::itoa(widths[k]);
//                    }
//                    TR("       %s", LCSTR(buf));

                    lastWidth += widths[len-1]; //len<m_length ? len : len-1];
                    m_flags[len] = 0;
                    // TODO: letter spacing letter_spacing
                } else {
                    // measure object
                    // assume i==start+1
                    int width = m_srcs[start]->o.width;
                    int height = m_srcs[start]->o.height;
                    resizeImage( width, height, m_pbuffer->width, m_pbuffer->page_height, m_length>1 );
                    lastWidth += width;
                    m_widths[start] = lastWidth;
                }
                start = i;
            }

            //
            lastFont = newFont;
            lastSrc = newSrc;
        }
        if ( tabIndex>=0 ) {
            int tabPosition = -m_srcs[0]->margin;
            if ( tabPosition>0 && tabPosition > m_widths[tabIndex] ) {
                int dx = tabPosition - m_widths[tabIndex];
                for ( i=tabIndex; i<m_length; i++ )
                    m_widths[i] += dx;
            }
        }
//        // debug dump
//        lString16 buf;
//        for ( int i=0; i<m_length; i++ ) {
//            buf << L" " << lChar16(m_text[i]) << L" " << lString16::itoa(m_widths[i]);
//        }
//        TR("%s", LCSTR(buf));
    }

#define MIN_WORD_LEN_TO_HYPHENATE 4
#define MAX_WORD_SIZE 64

    /// align line
    void alignLine( formatted_line_t * frmline, int width, int alignment ) {
        if ( frmline->x + frmline->width > width ) {
            // line is too wide
            // reduce spaces to fit line
            int extraSpace = frmline->x + frmline->width - width;
            if ( extraSpace<=0 )
                return; // no space to distribute
            int addSpacePoints = 0;
            int i;
            for ( i=0; i<(int)frmline->word_count-1; i++ ) {
                if ( frmline->words[i].flags & LTEXT_WORD_CAN_ADD_SPACE_AFTER )
                    addSpacePoints++;
            }
            if ( addSpacePoints>0 ) {
                int addSpaceDiv = extraSpace / addSpacePoints;
                int addSpaceMod = extraSpace % addSpacePoints;
                int delta = 0;
                for ( i=0; i<(int)frmline->word_count; i++ ) {
                    frmline->words[i].x -= delta;
                    if ( frmline->words[i].flags & LTEXT_WORD_CAN_ADD_SPACE_AFTER ) {
                        delta += addSpaceDiv;
                        if ( addSpaceMod>0 ) {
                            addSpaceMod--;
                            delta++;
                        }
                    }
                }
                frmline->width -= extraSpace;
            }
        } else if ( alignment==LTEXT_ALIGN_LEFT )
            return; // no additional alignment necessary
        else if ( alignment==LTEXT_ALIGN_CENTER ) {
            // centering, ignoring first line margin
            frmline->x = (width - frmline->width) / 2;
        } else if ( alignment==LTEXT_ALIGN_RIGHT ) {
            // right align
            frmline->x = (width - frmline->width);
        } else {
            // LTEXT_ALIGN_WIDTH
            int extraSpace = width - frmline->x - frmline->width;
            if ( extraSpace<=0 )
                return; // no space to distribute
            int addSpacePoints = 0;
            int i;
            for ( i=0; i<(int)frmline->word_count-1; i++ ) {
                if ( frmline->words[i].flags & LTEXT_WORD_CAN_ADD_SPACE_AFTER )
                    addSpacePoints++;
            }
            if ( addSpacePoints>0 ) {
                int addSpaceDiv = extraSpace / addSpacePoints;
                int addSpaceMod = extraSpace % addSpacePoints;
                int delta = 0;
                for ( i=0; i<(int)frmline->word_count; i++ ) {
                    frmline->words[i].x += delta;
                    if ( frmline->words[i].flags & LTEXT_WORD_CAN_ADD_SPACE_AFTER ) {
                        delta += addSpaceDiv;
                        if ( addSpaceMod>0 ) {
                            addSpaceMod--;
                            delta++;
                        }
                    }
                }
                frmline->width += extraSpace;
            }
        }
    }

    /// split line into words, add space for width alignment
    void addLine( int start, int end, int x, src_text_fragment_t * para, int interval, bool first, bool last, bool preFormattedOnly, bool needReduceSpace )
    {
        int maxWidth = m_pbuffer->width;
        //int w0 = start>0 ? m_widths[start-1] : 0;
        int align = para->flags & LTEXT_FLAG_NEWLINE;
        TR("addLine(%d, %d) y=%d  align=%d", start, end, m_y, align);

        int text_align_last = (para->flags >> LTEXT_LAST_LINE_ALIGN_SHIFT) & LTEXT_FLAG_NEWLINE;
        if ( last && !first && align==LTEXT_ALIGN_WIDTH && text_align_last!=0 )
            align = text_align_last;
        else if ( align==LTEXT_ALIGN_WIDTH && last )
            align = LTEXT_ALIGN_LEFT;
        if ( preFormattedOnly || !align )
            align = LTEXT_ALIGN_LEFT;

        bool visualAlignmentEnabled = gFlgFloatingPunctuationEnabled!=0 && (align == LTEXT_ALIGN_WIDTH || align == LTEXT_ALIGN_RIGHT );

        bool splitBySpaces = (align == LTEXT_ALIGN_WIDTH) || needReduceSpace;

        if ( last && !first ) {
            int last_align = (para->flags>>16) & LTEXT_FLAG_NEWLINE;
            if ( last_align )
                align = last_align;
        }

        int lastnonspace = 0;
        if ( align==LTEXT_ALIGN_WIDTH || splitBySpaces ) {
            for ( int i=start; i<end; i++ )
                if ( !((m_flags[i] & LCHAR_IS_SPACE) && !(m_flags[i] & LCHAR_IS_OBJECT)) )
                    lastnonspace = i;
        }

        formatted_line_t * frmline =  lvtextAddFormattedLine( m_pbuffer );
        frmline->y = m_y;
        frmline->x = x;
        src_text_fragment_t * lastSrc = m_srcs[start];
        int wstart = start;
        bool lastIsSpace = false;
        bool lastWord = false;
        bool isObject = false;
        bool isSpace = false;
        bool nextIsSpace = false;
        bool space = false;
        for ( int i=start; i<=end; i++ ) {
            src_text_fragment_t * newSrc = i<end ? m_srcs[i] : NULL;
            if ( i<end ) {
                isObject = (m_flags[i] & LCHAR_IS_OBJECT)!=0;
                isSpace = (m_flags[i] & LCHAR_IS_SPACE)!=0;
                nextIsSpace = i<end-1 && (m_flags[i+1] & LCHAR_IS_SPACE);
                space = splitBySpaces && lastIsSpace && !isSpace && i<lastnonspace;
            } else {
                lastWord = true;
            }
            if ( i>wstart && (newSrc!=lastSrc || space || lastWord) ) {
                // create and add new word
                formatted_word_t * word = lvtextAddFormattedWord(frmline);
                int b;
                int h;
                word->src_text_index = m_srcs[wstart]->index;
                if ( lastSrc->flags & LTEXT_SRC_IS_OBJECT ) {
                    // object
                    word->x = frmline->width;
                    word->y = 0;
                    word->flags = LTEXT_WORD_IS_OBJECT;
                    word->width = lastSrc->o.width;
                    word->o.height = lastSrc->o.height;
                    int maxw = m_pbuffer->width - x;

                    int width = lastSrc->o.width;
                    int height = lastSrc->o.height;
                    resizeImage( width, height, m_pbuffer->width - x, m_pbuffer->page_height, m_length>1 );
                    word->width = width;
                    word->o.height = height;

                    b = word->o.height;
                    h = 0;
                } else {
                    // word
                    src_text_fragment_t * srcline = m_srcs[wstart];
                    LVFont * font = (LVFont*)srcline->t.font;
                    int vertical_align = srcline->flags & LTEXT_VALIGN_MASK;
                    int fh = font->getSize();
                    int wy = 0;
                    if ( interval>16 )
                        wy = -((font->getSize() * (interval-16)) >> 4) >> 1;
                    else if ( interval<16 )
                        wy = ((font->getSize() * (16-interval)) >> 4) >> 1;
                    if ( vertical_align )  {
                        if ( vertical_align == LTEXT_VALIGN_SUB )
                            wy += fh / 3;
                        else if ( vertical_align == LTEXT_VALIGN_SUPER )
                            wy -= fh / 2;
                    }
                    word->x = frmline->width;
                    word->flags = 0;
                    word->t.start = m_charindex[wstart];
                    word->t.len = i - wstart;
                    word->width = m_widths[i>0 ? i-1 : 0] - (wstart>0 ? m_widths[wstart-1] : 0);
                    TR("addLine - word(%d, %d) x=%d (%d..%d)[%d] |%s|", wstart, i, frmline->width, wstart>0 ? m_widths[wstart-1] : 0, m_widths[i-1], word->width, LCSTR(lString16(m_text+wstart, i-wstart)));
                    if ( m_flags[i-1] & LCHAR_ALLOW_HYPH_WRAP_AFTER ) {
                        word->width += font->getHyphenWidth();
                        word->flags |= LTEXT_WORD_CAN_HYPH_BREAK_LINE_AFTER;
                    }
                    if ( m_flags[i-1] & LCHAR_IS_SPACE) {
                        word->flags |= LTEXT_WORD_CAN_ADD_SPACE_AFTER;
                        if ( !visualAlignmentEnabled && lastWord )
                            word->width = m_widths[i>1 ? i-2 : 0] - (wstart>0 ? m_widths[wstart-1] : 0);
                    } else if ( frmline->word_count>1 && m_flags[wstart] & LCHAR_IS_SPACE )
                        frmline->words[frmline->word_count-2].flags |= LTEXT_WORD_CAN_ADD_SPACE_AFTER;
                    if ( m_flags[i-1] & LCHAR_ALLOW_WRAP_AFTER )
                        word->flags |= LTEXT_WORD_CAN_BREAK_LINE_AFTER;
                    if ( word->t.start==0 && srcline->flags & LTEXT_IS_LINK )
                        word->flags |= LTEXT_WORD_IS_LINK_START;

                    if ( visualAlignmentEnabled && lastWord ) {
                        int endp = i-1;
                        int lastc = m_text[endp];
                        int wAlign = font->getVisualAligmentWidth();
                        word->width += wAlign;
                        while ( (lastc==' ') && endp>0 ) { // || lastc=='\r' || lastc=='\n'
                            word->width -= m_widths[endp] - m_widths[endp-1];
                            endp--;
                            lastc = m_text[endp];
                        }
                        if ( word->flags & LTEXT_WORD_CAN_HYPH_BREAK_LINE_AFTER ) {
                            word->width -= font->getHyphenWidth(); // TODO: strange fix - need some other solution
                        } else if ( lastc=='.' || lastc==',' || lastc=='!' || lastc==':'   || lastc==';' ) {
                            int w = font->getCharWidth(lastc);
                            TR("floating: %c w=%d", lastc, w);
                            word->width -= w;
                        }
                    }

                    word->y = wy;

//                    if (word->y!=0) {
//                        // subscript or superscript
                        b = font->getBaseline();
                        h = font->getHeight() - b;
//                    }  else  {
//                        b = (( font->getBaseline() * interval) >> 4);
//                        h = ( ( font->getHeight() * interval) >> 4) - b;
//                    }

                }

                if ( frmline->baseline < b - word->y )
                    frmline->baseline = (lUInt16) (b - word->y);
                if ( frmline->height < frmline->baseline + h )
                    frmline->height = (lUInt16) ( frmline->baseline + h );

                frmline->width += word->width;

                lastSrc = newSrc;
                wstart = i;
            }
            lastIsSpace = isSpace;
        }

        alignLine( frmline, maxWidth, align );

        m_y += frmline->height;
        m_pbuffer->height = m_y;
    }

    /// Split paragraph into lines
    void processParagraph( int start, int end )
    {
        TR("processParagraph(%d, %d)", start, end);

        // ensure buffer size is ok for paragraph
        allocate( start, end );
        // copy paragraph text to buffer
        copyText( start, end );
        // measure paragraph text
        measureText();

        // run-in detection
        src_text_fragment_t * para = &m_pbuffer->srctext[start];
        int i;
        for ( i=start; i<end; i++ ) {
            if ( !(m_pbuffer->srctext[i].flags & LTEXT_RUNIN_FLAG) ) {
                para = &m_pbuffer->srctext[i];
                break;
            }
        }

        // detect case with inline preformatted text inside block with line feeds -- override align=left for this case
        bool preFormattedOnly = true;
        for ( i=start; i<end; i++ ) {
            if ( !(m_pbuffer->srctext[i].flags & LTEXT_FLAG_PREFORMATTED) ) {
                preFormattedOnly = false;
                break;
            }
        }
        bool lfFound = false;
        for ( i=0; i<m_length; i++ ) {
            if ( m_text[i]=='\n' ) {
                lfFound = true;
                break;
            }
        }
        preFormattedOnly = preFormattedOnly && lfFound;

        int interval = m_srcs[0]->interval;
        int maxWidth = m_pbuffer->width;

#if 1
        // reservation of space for floating punctuation
        bool visualAlignmentEnabled = gFlgFloatingPunctuationEnabled!=0;
        int visialAlignmentWidth = 0;
        if ( visualAlignmentEnabled ) {
            LVFont * font = NULL;
            for ( int i=start; i<end; i++ ) {
                if ( !(m_srcs[i]->flags & LTEXT_SRC_IS_OBJECT) ) {
                    font = (LVFont*)m_srcs[i]->t.font;
                    int dx = font->getVisualAligmentWidth();
                    if ( dx>visialAlignmentWidth )
                        visialAlignmentWidth = dx;
                }
            }
            maxWidth -= visialAlignmentWidth;
        }
#endif

        // split paragraph into lines, export lines
        int pos = 0;
        int indent = m_srcs[0]->margin;
        for (;pos<m_length;) {
            int x = indent >=0 ? (pos==0 ? indent : 0) : (pos==0 ? 0 : -indent);
            int w0 = pos>0 ? m_widths[pos-1] : 0;
            int i;
            int lastNormalWrap = -1;
            int lastDeprecatedWrap = -1;
            int lastHyphWrap = -1;
            int lastMandatoryWrap = -1;
            int spaceReduceWidth = 0; // max total line width which can be reduced by narrowing of spaces
            for ( i=pos; i<m_length; i++ ) {
                if ( x + m_widths[i]-w0 > maxWidth + spaceReduceWidth )
                    break;
                lUInt8 flags = m_flags[i];
                if ( m_text[i]=='\n' ) {
                    lastMandatoryWrap = i;
                    break;
                }
                if ( flags & LCHAR_ALLOW_WRAP_AFTER || i==m_length-1)
                    lastNormalWrap = i;
                else if ( flags & LCHAR_DEPRECATED_WRAP_AFTER )
                    lastDeprecatedWrap = i;
                else if ( flags & LCHAR_ALLOW_HYPH_WRAP_AFTER )
                    lastHyphWrap = i;
                if ( i<m_length-1 && m_text[i]==' ' && m_text[i+1]!=' ' ) {
                    spaceReduceWidth += (m_widths[i] - m_widths[i-1]) / 2;
                }
            }
            if ( i<=pos )
                i = pos + 1; // allow at least one character to be shown on line
            int wordpos = i-1;
            if ( lastNormalWrap<0 && lastDeprecatedWrap>=0 )
                lastNormalWrap = lastDeprecatedWrap;
            int normalWrapWidth = lastNormalWrap > 0 ? x + m_widths[lastNormalWrap]-w0 : 0;
            int unusedSpace = maxWidth - normalWrapWidth;
            if ( lastMandatoryWrap<0 && lastNormalWrap<m_length-1 && unusedSpace > maxWidth/10 && !(m_srcs[wordpos]->flags & LTEXT_SRC_IS_OBJECT) && (m_srcs[wordpos]->flags & LTEXT_HYPHENATE) ) {
                // hyphenate word
                int start, end;
                lStr_findWordBounds( m_text, m_length, wordpos, start, end );
                int len = end-start;
                if ( len>0 )
                    TR("wordBounds(%s) unusedSpace=%d wordWidth=%d", LCSTR(lString16(m_text+start, len)), unusedSpace, m_widths[end]-m_widths[start]);
                if ( start<end && start<wordpos && end>=wordpos && len>=MIN_WORD_LEN_TO_HYPHENATE ) {
                    if ( len > MAX_WORD_SIZE )
                        len = MAX_WORD_SIZE;
                    lUInt8 * flags = m_flags + start;
                    static lUInt16 widths[MAX_WORD_SIZE];
                    int wordStart_w = start>0 ? m_widths[start-1] : 0;
                    for ( int i=0; i<len; i++ ) {
                        widths[i] = m_widths[start+i] - wordStart_w;
                    }
                    int max_width = maxWidth + spaceReduceWidth - x - (wordStart_w - w0);
                    int _hyphen_width = ((LVFont*)m_srcs[wordpos]->t.font)->getHyphenWidth();
                    if ( HyphMan::hyphenate(m_text+start, len, widths, flags, _hyphen_width, max_width) ) {
                        for ( int i=0; i<len; i++ )
                            if ( (m_flags[start+i] & LCHAR_ALLOW_HYPH_WRAP_AFTER)!=0 ) {
                                if ( widths[i]+_hyphen_width>max_width ) {
                                    TR("hyphen found, but max width reached at char %d", i);
                                    break; // hyph is too late
                                }
                                if ( start + i > pos+1 )
                                    lastHyphWrap = start + i;
                            }
                    } else {
                        TR("no hyphen found - max_width=%d", max_width);
                    }
                }
            }
            int wrapPos = lastHyphWrap;
            if ( lastMandatoryWrap>=0 )
                wrapPos = lastMandatoryWrap;
            else {
                if ( wrapPos<lastNormalWrap )
                    wrapPos = lastNormalWrap;
                if ( wrapPos<0 )
                    wrapPos = i-1;
            }
            bool needReduceSpace = true; // todo: calculate whether space reducing required
            addLine(pos, wrapPos+(lastMandatoryWrap<0 ? 1 : 0), x, para, interval, pos==0, wrapPos>=m_length-1, preFormattedOnly, needReduceSpace );
            pos = wrapPos + 1;
        }
    }

    /// split source data into paragraphs
    void splitParagraphs()
    {
        int start = 0;
        int i;
//        TR("==== splitParagraphs() ====");
//        for ( i=0; i<m_pbuffer->srctextlen; i++ ) {
//            int flg = m_pbuffer->srctext[i].flags;
//            if ( (flg & LTEXT_RUNIN_FLAG) )
//                TR("run-in found");
//            TR("  %d: flg=%04x al=%d ri=%d '%s'", i, flg, (flg & LTEXT_FLAG_NEWLINE), (flg & LTEXT_RUNIN_FLAG)?1:0, (flg&LTEXT_SRC_IS_OBJECT ? "<image>" : LCSTR(lString16(m_pbuffer->srctext[i].t.text, m_pbuffer->srctext[i].t.len)) ) );
//        }
//        TR("============================");
        bool prevRunIn = m_pbuffer->srctextlen>0 && (m_pbuffer->srctext[0].flags & LTEXT_RUNIN_FLAG);
        for ( i=1; i<=m_pbuffer->srctextlen; i++ ) {
            if ( (i==m_pbuffer->srctextlen) || ((m_pbuffer->srctext[i].flags & LTEXT_FLAG_NEWLINE) && !prevRunIn) ) {
                processParagraph( start, i );
                start = i;
            }
            prevRunIn = (i<m_pbuffer->srctextlen) && (m_pbuffer->srctext[i].flags & LTEXT_RUNIN_FLAG);
        }
    }

    void dealloc()
    {
        if ( !m_staticBufs ) {
            free( m_text );
            free( m_flags );
            free( m_srcs );
            free( m_charindex );
            free( m_widths );
            m_text = NULL;
            m_flags = NULL;
            m_srcs = NULL;
            m_charindex = NULL;
            m_widths = NULL;
            m_staticBufs = true;
        }
    }

    /// format source data
    int format()
    {
        // split and process all paragraphs
        splitParagraphs();
        // cleanup
        dealloc();
        TR("format() finished: h=%d  lines=%d", m_y, m_pbuffer->frmlinecount);
        return m_y;
    }
};

static void freeFrmLines( formatted_text_fragment_t * m_pbuffer )
{
    // clear existing formatted data, if any
    if (m_pbuffer->frmlines)
    {
        for (lUInt32 i=0; i<m_pbuffer->frmlinecount; i++)
        {
            lvtextFreeFormattedLine( m_pbuffer->frmlines[i] );
        }
        free( m_pbuffer->frmlines );
    }
    m_pbuffer->frmlines = NULL;
    m_pbuffer->frmlinecount = 0;
}

// experimental formatter
lUInt32 LFormattedText::Format(lUInt16 width, lUInt16 page_height)
{
    // clear existing formatted data, if any
    freeFrmLines( m_pbuffer );
    // setup new page size
    m_pbuffer->width = width;
    m_pbuffer->height = 0;
    m_pbuffer->page_height = page_height;
    // format text
    LVFormatter formatter( m_pbuffer );

    return formatter.format();
}

void LFormattedText::setImageScalingOptions( img_scaling_options_t * options )
{
    m_pbuffer->img_zoom_in_mode_block = options->zoom_in_block.mode;
    m_pbuffer->img_zoom_in_scale_block = options->zoom_in_block.max_scale;
    m_pbuffer->img_zoom_in_mode_inline = options->zoom_in_inline.mode;
    m_pbuffer->img_zoom_in_scale_inline = options->zoom_in_inline.max_scale;
    m_pbuffer->img_zoom_out_mode_block = options->zoom_out_block.mode;
    m_pbuffer->img_zoom_out_scale_block = options->zoom_out_block.max_scale;
    m_pbuffer->img_zoom_out_mode_inline = options->zoom_out_inline.mode;
    m_pbuffer->img_zoom_out_scale_inline = options->zoom_out_inline.max_scale;
}

void LFormattedText::Draw( LVDrawBuf * buf, int x, int y, ldomMarkedRangeList * marks, ldomMarkedRangeList *bookmarks )
{
    lUInt32 i, j;
    formatted_line_t * frmline;
    src_text_fragment_t * srcline;
    formatted_word_t * word;
    LVFont * font;
    lvRect clip;
    buf->GetClipRect( &clip );
    const lChar16 * str;
    int line_y = y;
    for (i=0; i<m_pbuffer->frmlinecount; i++)
    {
        if (line_y>=clip.bottom)
            break;
        frmline = m_pbuffer->frmlines[i];
        if (line_y + frmline->height>=clip.top)
        {
            // process background

            lUInt32 bgcl = buf->GetBackgroundColor();
            //buf->FillRect( x+frmline->x, y + frmline->y, x+frmline->x + frmline->width, y + frmline->y + frmline->height, bgcl );

            // draw background for each word
            lUInt32 lastWordColor = 0xFFFFFFFF;
            int lastWordStart = -1;
            int lastWordEnd = -1;
            for (j=0; j<frmline->word_count; j++)
            {
                word = &frmline->words[j];
                srcline = &m_pbuffer->srctext[word->src_text_index];
                if (word->flags & LTEXT_WORD_IS_OBJECT)
                {
                    // no background, TODO
                }
                else
                {
                    lUInt32 bgcl = srcline->bgcolor;
                    if ( lastWordColor!=bgcl || lastWordStart==-1 ) {
                        if ( lastWordStart!=-1 )
                            if ( ((lastWordColor>>24) & 0xFF) < 128 )
                                buf->FillRect( lastWordStart, y + frmline->y, lastWordEnd, y + frmline->y + frmline->height, lastWordColor );
                        lastWordColor=bgcl;
                        lastWordStart = x+frmline->x+word->x;
                    }
                    lastWordEnd = x+frmline->x+word->x+word->width;
                }
            }
            if ( lastWordStart!=-1 )
                if ( ((lastWordColor>>24) & 0xFF) < 128 )
                    buf->FillRect( lastWordStart, y + frmline->y, lastWordEnd, y + frmline->y + frmline->height, lastWordColor );

            // process marks
#ifndef CR_USE_INVERT_FOR_SELECTION_MARKS
            if ( marks!=NULL && marks->length()>0 ) {
                lvRect lineRect( frmline->x, frmline->y, frmline->x + frmline->width, frmline->y + frmline->height );
                for ( int i=0; i<marks->length(); i++ ) {
                    lvRect mark;
                    ldomMarkedRange * range = marks->get(i);
                    if ( range->intersects( lineRect, mark ) ) {
                        //
                        buf->FillRect( mark.left + x, mark.top + y, mark.right + x, mark.bottom + y, 0xAAAAAA );
                    }
                }
            }
#endif
#ifdef CR_USE_INVERT_FOR_SELECTION_MARKS
            // process bookmarks
            if ( bookmarks != NULL && bookmarks->length() > 0 ) {
                lvRect lineRect( frmline->x, frmline->y, frmline->x + frmline->width, frmline->y + frmline->height );
                for ( int i=0; i<bookmarks->length(); i++ ) {
                    lvRect bookmark_rc;
                    ldomMarkedRange * range = bookmarks->get(i);
                    if ( range->intersects( lineRect, bookmark_rc ) ) {
                        buf->FillRect( bookmark_rc.left + x, bookmark_rc.top + y, bookmark_rc.right + x, bookmark_rc.bottom + y, 0xAAAAAA );
                    }
                }
            }
#endif
            for (j=0; j<frmline->word_count; j++)
            {
                word = &frmline->words[j];
                if (word->flags & LTEXT_WORD_IS_OBJECT)
                {
                    srcline = &m_pbuffer->srctext[word->src_text_index];
                    ldomNode * node = (ldomNode *) srcline->object;
                    LVImageSourceRef img = node->getObjectImageSource();
                    if ( img.isNull() )
                        img = LVCreateDummyImageSource( node, word->width, word->o.height );
                    int xx = x + frmline->x + word->x;
                    int yy = line_y + frmline->baseline - word->o.height + word->y;
                    buf->Draw( img, xx, yy, word->width, word->o.height );
                    //buf->FillRect( xx, yy, xx+word->width, yy+word->height, 1 );
                }
                else
                {
                    bool flgHyphen = false;
                    if ( (j==frmline->word_count-1) &&
                        (word->flags&LTEXT_WORD_CAN_HYPH_BREAK_LINE_AFTER))
                        flgHyphen = true;
                    srcline = &m_pbuffer->srctext[word->src_text_index];
                    font = (LVFont *) srcline->t.font;
                    str = srcline->t.text + word->t.start;
                    /*
                    lUInt32 srcFlags = srcline->flags;
                    if ( srcFlags & LTEXT_BACKGROUND_MARK_FLAGS ) {
                        lvRect rc;
                        rc.left = x + frmline->x + word->x;
                        rc.top = line_y + (frmline->baseline - font->getBaseline()) + word->y;
                        rc.right = rc.left + word->width;
                        rc.bottom = rc.top + font->getHeight();
                        buf->FillRect( rc.left, rc.top, rc.right, rc.bottom, 0xAAAAAA );
                    }
                    */
                    lUInt32 oldColor = buf->GetTextColor();
                    lUInt32 oldBgColor = buf->GetBackgroundColor();
                    lUInt32 cl = srcline->color;
                    lUInt32 bgcl = srcline->bgcolor;
                    if ( cl!=0xFFFFFFFF )
                        buf->SetTextColor( cl );
                    if ( bgcl!=0xFFFFFFFF )
                        buf->SetBackgroundColor( bgcl );
                    font->DrawTextString(
                        buf,
                        x + frmline->x + word->x,
                        line_y + (frmline->baseline - font->getBaseline()) + word->y,
                        str,
                        word->t.len,
                        '?',
                        NULL,
                        flgHyphen,
                        srcline->flags & 0x0F00,
                        srcline->letter_spacing);
                    if ( cl!=0xFFFFFFFF )
                        buf->SetTextColor( oldColor );
                    if ( bgcl!=0xFFFFFFFF )
                        buf->SetBackgroundColor( oldBgColor );
                }
            }

#ifdef CR_USE_INVERT_FOR_SELECTION_MARKS
            // process marks
            if ( marks!=NULL && marks->length()>0 ) {
                lvRect lineRect( frmline->x, frmline->y, frmline->x + frmline->width, frmline->y + frmline->height );
                for ( int i=0; i<marks->length(); i++ ) {
                    lvRect mark;
                    ldomMarkedRange * range = marks->get(i);
                    if ( range->intersects( lineRect, mark ) ) {
						buf->InvertRect( mark.left + x, mark.top + y, mark.right + x, mark.bottom + y);
                    }
                }
            }
#endif
        }
        line_y += frmline->height;
    }
}

#endif
