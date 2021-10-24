//   Copyright (c) 2003-2007 Clarence Dang <dang@kde.org>
//   Copyright (c) 2006 Mike Gashler <gashlerm@yahoo.com>
//   All rights reserved.

//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:

//   1. Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//   2. Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.

//   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
//   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
//   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
//   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
//   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
//   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef IMGEFFECTS_H
#define IMGEFFECTS_H

#include <QImage>

inline QRgb convertFromPremult(QRgb p)
{
    int alpha = qAlpha(p);
    return(!alpha ? 0 : qRgba(255*qRed(p)/alpha,
                              255*qGreen(p)/alpha,
                              255*qBlue(p)/alpha,
                              alpha));
}

inline QRgb convertToPremult(QRgb p)
{
    unsigned int a = p >> 24;
    unsigned int t = (p & 0xff00ff) * a;
    t = (t + ((t >> 8) & 0xff00ff) + 0x800080) >> 8;
    t &= 0xff00ff;

    p = ((p >> 8) & 0xff) * a;
    p = (p + ((p >> 8) & 0xff) + 0x80);
    p &= 0xff00;
    p |= t | (a << 24);
    return(p);
}

// These are used as accumulators

typedef struct
{
    quint32 red, green, blue, alpha;
} IntegerPixel;

typedef struct
{
    quint16 red, green, blue, alpha;
} ShortPixel;

typedef struct
{
    // Yes, a normal pixel can be used instead but this is easier to read
    // and no shifts to get components.
    quint8 red, green, blue, alpha;
} CharPixel;

typedef struct
{
    quint32 red, green, blue, alpha;
} HistogramListItem;

static bool normalize(QImage &img)
{
    if(img.isNull())
        return(false);

    IntegerPixel intensity;
    HistogramListItem *histogram;
    CharPixel *normalize_map;
    ShortPixel high, low;

    uint threshold_intensity;
    int i, count;
    QRgb pixel, *dest;
    unsigned char r, g, b;

    if(img.depth() < 32){
        img = img.convertToFormat(img.hasAlphaChannel() ?
                                  QImage::Format_ARGB32 :
                                  QImage::Format_RGB32);
    }
    count = img.width()*img.height();

    histogram = new HistogramListItem[256];
    normalize_map = new CharPixel[256];

    // form histogram
    memset(histogram, 0, 256*sizeof(HistogramListItem));
    dest = (QRgb *)img.bits();

    if(img.format() == QImage::Format_ARGB32_Premultiplied){
        for(i=0; i < count; ++i, ++dest){
            pixel = convertFromPremult(*dest);
            histogram[qRed(pixel)].red++;
            histogram[qGreen(pixel)].green++;
            histogram[qBlue(pixel)].blue++;
            histogram[qAlpha(pixel)].alpha++;
        }
    }
    else{
        for(i=0; i < count; ++i){
            pixel = *dest++;
            histogram[qRed(pixel)].red++;
            histogram[qGreen(pixel)].green++;
            histogram[qBlue(pixel)].blue++;
            histogram[qAlpha(pixel)].alpha++;
        }
    }

    // find the histogram boundaries by locating the .01 percent levels.
    threshold_intensity = count/100;
//    threshold_intensity = count/1000;

    memset(&intensity, 0, sizeof(IntegerPixel));
    for(low.red=0; low.red < 256; ++low.red){
        intensity.red += histogram[low.red].red;
        if(intensity.red > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(high.red=256; high.red-- > 0; ){
        intensity.red += histogram[high.red].red;
        if(intensity.red > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(low.green=low.red; low.green < high.red; ++low.green){
        intensity.green += histogram[low.green].green;
        if(intensity.green > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(high.green=high.red; high.green != low.red; --high.green){
        intensity.green += histogram[high.green].green;
        if(intensity.green > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(low.blue=low.green; low.blue < high.green; ++low.blue){
        intensity.blue += histogram[low.blue].blue;
        if(intensity.blue > threshold_intensity)
            break;
    }
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(high.blue=high.green; high.blue != low.green; --high.blue){
        intensity.blue += histogram[high.blue].blue;
        if(intensity.blue > threshold_intensity)
            break;
    }

    delete[] histogram;

    // stretch the histogram to create the normalized image mapping.
    for(i=0; i < 256; i++){
        if(i < low.red)
            normalize_map[i].red = 0;
        else{
            if(i > high.red)
                normalize_map[i].red = 255;
            else if(low.red != high.red)
                normalize_map[i].red = (255*(i-low.red))/
                    (high.red-low.red);
        }

        if(i < low.green)
            normalize_map[i].green = 0;
        else{
            if(i > high.green)
                normalize_map[i].green = 255;
            else if(low.green != high.green)
                normalize_map[i].green = (255*(i-low.green))/
                    (high.green-low.green);
        }

        if(i < low.blue)
            normalize_map[i].blue = 0;
        else{
            if(i > high.blue)
                normalize_map[i].blue = 255;
            else if(low.blue != high.blue)
                normalize_map[i].blue = (255*(i-low.blue))/
                    (high.blue-low.blue);
        }
    }

    // write
    dest = (QRgb *)img.bits();
    if(img.format() == QImage::Format_ARGB32_Premultiplied){
        for(i=0; i < count; ++i, ++dest){
            pixel = convertFromPremult(*dest);
            r = (low.red != high.red) ? normalize_map[qRed(pixel)].red :
                qRed(pixel);
            g = (low.green != high.green) ? normalize_map[qGreen(pixel)].green :
                qGreen(pixel);
            b = (low.blue != high.blue) ?  normalize_map[qBlue(pixel)].blue :
                qBlue(pixel);
            *dest = convertToPremult(qRgba(r, g, b, qAlpha(pixel)));
        }
    }
    else{
        for(i=0; i < count; ++i){
            pixel = *dest;
            r = (low.red != high.red) ? normalize_map[qRed(pixel)].red :
                qRed(pixel);
            g = (low.green != high.green) ? normalize_map[qGreen(pixel)].green :
                qGreen(pixel);
            b = (low.blue != high.blue) ?  normalize_map[qBlue(pixel)].blue :
                qBlue(pixel);
            *dest++ = qRgba(r, g, b, qAlpha(pixel));
        }
    }

    delete[] normalize_map;
    return(true);
}

static bool equalize(QImage &img)
{
    if(img.isNull())
        return(false);

    HistogramListItem *histogram;
    IntegerPixel *map;
    IntegerPixel intensity, high, low;
    CharPixel *equalize_map;
    int i, count;
    QRgb pixel, *dest;
    unsigned char r, g, b;

    if(img.depth() < 32){
        img = img.convertToFormat(img.hasAlphaChannel() ?
                                  QImage::Format_ARGB32 :
                                  QImage::Format_RGB32);
    }
    count = img.width()*img.height();

    map = new IntegerPixel[256];
    histogram = new HistogramListItem[256];
    equalize_map = new CharPixel[256];

    // form histogram
    memset(histogram, 0, 256*sizeof(HistogramListItem));
    dest = (QRgb *)img.bits();

    if(img.format() == QImage::Format_ARGB32_Premultiplied){
        for(i=0; i < count; ++i, ++dest){
            pixel = convertFromPremult(*dest);
            histogram[qRed(pixel)].red++;
            histogram[qGreen(pixel)].green++;
            histogram[qBlue(pixel)].blue++;
            histogram[qAlpha(pixel)].alpha++;
        }
    }
    else{
        for(i=0; i < count; ++i){
            pixel = *dest++;
            histogram[qRed(pixel)].red++;
            histogram[qGreen(pixel)].green++;
            histogram[qBlue(pixel)].blue++;
            histogram[qAlpha(pixel)].alpha++;
        }
    }

    // integrate the histogram to get the equalization map
    memset(&intensity, 0, sizeof(IntegerPixel));
    for(i=0; i < 256; ++i){
        intensity.red += histogram[i].red;
        intensity.green += histogram[i].green;
        intensity.blue += histogram[i].blue;
        map[i] = intensity;
    }

    low = map[0];
    high = map[255];
    memset(equalize_map, 0, 256*sizeof(CharPixel));
    for(i=0; i < 256; ++i){
        if(high.red != low.red)
            equalize_map[i].red=(unsigned char)
                ((255*(map[i].red-low.red))/(high.red-low.red));
        if(high.green != low.green)
            equalize_map[i].green=(unsigned char)
                ((255*(map[i].green-low.green))/(high.green-low.green));
        if(high.blue != low.blue)
            equalize_map[i].blue=(unsigned char)
                ((255*(map[i].blue-low.blue))/(high.blue-low.blue));
    }

    // stretch the histogram and write
    dest = (QRgb *)img.bits();
    if(img.format() == QImage::Format_ARGB32_Premultiplied){
        for(i=0; i < count; ++i, ++dest){
            pixel = convertFromPremult(*dest);
            r = (low.red != high.red) ? equalize_map[qRed(pixel)].red :
                qRed(pixel);
            g = (low.green != high.green) ? equalize_map[qGreen(pixel)].green :
                qGreen(pixel);
            b = (low.blue != high.blue) ?  equalize_map[qBlue(pixel)].blue :
                qBlue(pixel);
            *dest = convertToPremult(qRgba(r, g, b, qAlpha(pixel)));
        }
    }
    else{
        for(i=0; i < count; ++i){
            pixel = *dest;
            r = (low.red != high.red) ? equalize_map[qRed(pixel)].red :
                qRed(pixel);
            g = (low.green != high.green) ? equalize_map[qGreen(pixel)].green :
                qGreen(pixel);
            b = (low.blue != high.blue) ?  equalize_map[qBlue(pixel)].blue :
                qBlue(pixel);
            *dest++ = qRgba(r, g, b, qAlpha(pixel));
        }
    }

    delete[] histogram;
    delete[] map;
    delete[] equalize_map;
    return(true);
}

#endif // IMGEFFECTS_H
