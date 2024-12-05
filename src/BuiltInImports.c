#include "BuiltInImports.h"

#include <string.h>

#define RAW_STRING_LITERAL(string) #string

//@formatter:off

// optional parameters
// out parameters
// passing in parameters as reference
// variadic parameters
// assigning to r-value
// special behaviour with slider variables
// buffers as parameters
// could be implemented differently

static const char jsfx[] = RAW_STRING_LITERAL(
    public import "math"
    public import "str"
    public import "gfx"

    public external int time();
    public external float time_precise();

    public external float spl0;
    public external float spl1;
    public external float spl2;
    public external float spl3;
    public external float spl4;
    public external float spl5;
    public external float spl6;
    public external float spl7;
    public external float spl8;
    public external float spl9;
    public external float spl10;
    public external float spl11;
    public external float spl12;
    public external float spl13;
    public external float spl14;
    public external float spl15;
    public external float spl16;
    public external float spl17;
    public external float spl18;
    public external float spl19;
    public external float spl20;
    public external float spl21;
    public external float spl22;
    public external float spl23;
    public external float spl24;
    public external float spl25;
    public external float spl26;
    public external float spl27;
    public external float spl28;
    public external float spl29;
    public external float spl30;
    public external float spl31;
    public external float spl32;
    public external float spl33;
    public external float spl34;
    public external float spl35;
    public external float spl36;
    public external float spl37;
    public external float spl38;
    public external float spl39;
    public external float spl40;
    public external float spl41;
    public external float spl42;
    public external float spl43;
    public external float spl44;
    public external float spl45;
    public external float spl46;
    public external float spl47;
    public external float spl48;
    public external float spl49;
    public external float spl50;
    public external float spl51;
    public external float spl52;
    public external float spl53;
    public external float spl54;
    public external float spl55;
    public external float spl56;
    public external float spl57;
    public external float spl58;
    public external float spl59;
    public external float spl60;
    public external float spl61;
    public external float spl62;
    public external float spl63;

    // spl()

    // slider variables
    // slider()
    // sliderchange()
    // slider_next_chg(sliderindex,nextval)
    // slider_automate(mask or sliderX[, end_touch])
    // slider_automate(mask or sliderX[, end_touch])

    // trigger

    public external int srate;
    public external int num_ch;
    public external int samplesblock;
    public external float tempo;
    // play_state
    public external float play_position;
    public external float beat_position;
    public external float ts_num;
    public external float ts_denom;

    public external bool ext_noinit;
    public external bool ext_nodenorm;
    public external int ext_tail_size;
    // reg variables? no
    // _global variables

    public external int pdc_delay;
    public external int pdc_bot_ch;
    public external int pdc_top_ch;
    public external bool pdc_midi;

    public external int midisend(int offset, int msg1, int msg2, int msg3);
    // midisend_buf(offset,buf, len)
    public external int midisend_str(int offset, string str);
    // midirecv(offset,msg1,msg2,msg3)
    // midirecv_buf(offset,buf, maxlen)
    // midirecv_str(offset, string)

    public external bool ext_midi_bus;
    public external int midi_bus;

    // file_open(index or slider)
    public external void file_close(float handle);
    public external void file_rewind(float handle);
    // file_var(handle,variable)
    // file_mem(handle,offset, length)
    public external int file_avail(float handle);
    // file_riff(handle,nch,samplrate)
    public external bool file_text(float handle);
    // file_string(handle,str)

    // mdct(start_index, size), imdct(start_index, size)
    // fft(start_index, size), ifft(start_index, size)
    // fft_real(start_index, size), ifft_real(start_index, size)
    // fft_permute(index,size), fft_ipermute(index,size)
    // convolve_c(dest,src,size)

    // freembuf(top)
    // memcpy(dest,source,length)
    // memset(dest,value,length)
    // mem_set_values(buf, ...) -- REAPER 5.28+
    // mem_get_values(buf, ...) -- REAPER 5.28+
    // mem_multiply_sum(buf1,buf2,length) -- REAPER 6.74+
    // mem_insert_shuffle(buf,len,value) -- REAPER 6.74+
    // __memtop()

    public external void stack_push(float value);
    public external float stack_pop();
    public external float stack_peek(int index);
    public external float stack_exch(float value);

    // atomic_setifequal(dest,value,newvalue)
    // atomic_exch(val1,val2)
    // atomic_add(dest_val1,val2)
    // atomic_set(dest_val1,val2)
    // atomic_get(val)

    // export_buffer_to_project(buffer,length_samples,nch,srate,track_index[,flags,tempo,planar_pitch])

    public external int get_host_numchan();
    public external void set_host_numchan(int numchan);
    // get_pin_mapping(inout,pin,startchan,chanmask)
    // set_pin_mapping(inout,pin,startchan,chanmask,mapping)
    // get_pinmapper_flags()
    // set_pinmapper_flags(flags);

    // get_host_placement([chain_pos, flags])
);

static const char math[] = RAW_STRING_LITERAL(
    public external float sin(float angle);
    public external float cos(float angle);
    public external float tan(float angle);
    public external float asin(float x);
    public external float acos(float x);
    public external float atan(float x);
    public external float atan2(float x, float y);
    public external float sqr(float x);
    public external float sqrt(float x);
    public external float pow(float x, float y);
    public external float exp(float x);
    public external float log(float x);
    public external float log10(float x);
    public external float abs(float x);
    public external float min(float x, float y);
    public external float max(float x, float y);
    public external float sign(float x);
    public external float rand(float x);
    public external float floor(float x);
    public external float ceil(float x);
    public external float invsqrt(float x);

    public float lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }
);

static const char string[] = RAW_STRING_LITERAL(
    public external int strlen(string str);
    public external string strcpy(string str, string srcstr);
    public external string strcat(string str, string srcstr);
    public external int strcmp(string str, string str2);
    public external int stricmp(string str, string str2);
    public external int strncmp(string str, string str2, int maxlen);
    public external int strnicmp(string str, string str2, int maxlen);
    public external string strncpy(string str, string srcstr, int maxlen);
    public external string strncat(string str, string srcstr, int maxlen);
    public external string strcpy_from(string str, string srcstr, int offset);
    public external string strcpy_substr(string str, string srcstr, int offset, int maxlen);
    // str_getchar(str, offset[, type]) -- returns the data at byte-offset offset of str. if offset is negative, position is relative to end of string. Type defaults to signed char, but can be specified to read raw binary data in other formats (note the single quotes, these are single/multi-byte characters):
    // str_setchar(str, offset, value[, type]) -- sets the value at byte-offset "offset" of str to value (which may be one or more bytes of data). If offset is negative, then offset is relative to end of the string. If offset is the length of the string, or between (-0.5,0.0), then the character (or multibyte value if type is specified) will be appended to the string.
    // strcpy_fromslider(str, slider) -- gets the filename if a file-slider, or the string if the slider specifies string translations, otherwise gets an empty string. slider can be either an index, or the sliderX variable directly. returns str.
    // sprintf(str,format, ...) -- copies format to str, converting format strings:
    // match(needle, haystack, ...) -- search for needle in haystack
    // matchi(needle, haystack, ...)
);

static const char gfx[] = RAW_STRING_LITERAL(
    public import "mouse"

    public struct Color
    {
        float r;
        float g;
        float b;
    }

    public external float r gfx_r;
    public external float g gfx_g;
    public external float b gfx_b;
    public external float a gfx_a;

    public external int w gfx_w;
    public external int h gfx_h;

    public external int x gfx_x;
    public external int y gfx_y;

    public external int mode gfx_mode;
    public external int clear gfx_clear;
    public external int dest gfx_dest;
    public external int texth gfx_texth;
    public external float ext_retina gfx_ext_retina;
    public external int ext_flags gfx_ext_flags;

    //public external void gfx_set(r[g,b,a,mode,dest]);
    //public external void gfx_lineto(x,y,aa);
    //public external void gfx_line(x,y,x2,y2[,aa]);
    public external void gfx_rectto(int x, int y);
    public external void rect(int x, int y, int w, int h) gfx_rect;
    //public external void gfx_setpixel(r,g,b);
    //public external void gfx_drawnumber(n,ndigits);
    //public external void gfx_drawchar($'c');
    //public external void gfx_drawstr(str[,flags,right,bottom]);
    //public external void gfx_measurestr(str,w,h);
    //public external void gfx_setfont(idx[,fontface, sz, flags]);
    //gfx_getfont();
    //public external void gfx_printf(str, ...);
    //public external void gfx_blurto(x,y);
    //public external void gfx_blit(source, scale, rotation);
    //public external void gfx_blit(source, scale, rotation[, srcx, srcy, srcw, srch, destx, desty, destw, desth, rotxoffs, rotyoffs]);
    //public external void gfx_blitext(source, coordinatelist, rotation);
    //gfx_getimgdim(image, w, h);
    //gfx_setimgdim(image, w,h);
    //gfx_loadimg(image, filename);
    //public external void gfx_gradrect(x,y,w,h, r,g,b,a[, drdx, dgdx, dbdx, dadx, drdy, dgdy, dbdy, dady]);
    //public external void gfx_muladdrect(x,y,w,h, mul_r, mul_g, mul_b[, mul_a, add_r, add_g, add_b, add_a]);
    //public external void gfx_deltablit(srcimg,srcx,srcy,srcw,srch, destx, desty, destw, desth, dsdx, dtdx, dsdy, dtdy, dsdxdy, dtdxdy[, usecliprect=1] );
    //public external void gfx_transformblit(srcimg, destx, desty, destw, desth, div_w, div_h, table);
    //public external void gfx_circle(x,y,r[,fill,antialias]);
    //public external void gfx_roundrect(x,y,w,h,radius[,antialias]);
    //public external void gfx_arc(x,y,r, ang1, ang2[,antialias]);
    //public external void gfx_triangle(x1,y1,x2,y2,x3,y3[,x4,y4,...]);
    //gfx_getchar([char, unicodechar]);
    //gfx_showmenu("str");
    //public external void gfx_setcursor(resource_id[,"custom cursor name"]);

    external int gfx_x;
    external int gfx_y;
    public void set_pos(int x, int y)
    {
        gfx_x = x;
        gfx_y = y;
    }

    external void gfx_getpixel(float r, float g, float b);
    public Color getpixel()
    {
        Color color;
        gfx_getpixel(color.r, color.g, color.b);
        return color;
    }
);

static const char mouse[] = RAW_STRING_LITERAL(

    public external int x mouse_x;
    public external int y mouse_y;
    public external int cap mouse_cap;
    public external int wheel mouse_wheel;
    public external int hwheel mouse_hwheel;
);

const char* GetBuiltInSource(const char* path)
{
    if (strcmp(path, "jsfx") == 0) return jsfx;
    if (strcmp(path, "math") == 0) return math;
    if (strcmp(path, "str") == 0) return string;
    if (strcmp(path, "gfx") == 0) return gfx;
    if (strcmp(path, "mouse") == 0) return mouse;
    return NULL;
}
