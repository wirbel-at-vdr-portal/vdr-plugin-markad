/*
 * global.h: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#ifndef __global_h_
#define __global_h_

#include <time.h>

#ifndef uchar
typedef unsigned char uchar;
#endif

#define MA_I_TYPE 1
#define MA_P_TYPE 2
#define MA_B_TYPE 3
#define MA_S_TYPE 4
#define MA_SI_TYPE 5
#define MA_SP_TYPE 6
#define MA_BI_TYPE 7
#define MA_D_TYPE 80

typedef struct MarkAdMark
{
    int Position;
    char *Comment;
} MarkAdMark;

typedef struct MarkAdAspectRatio
{
    int Num;
    int Den;
} MarkAdAspectRatio;

#define MARKAD_PIDTYPE_VIDEO_H262 0x10
#define MARKAD_PIDTYPE_VIDEO_H264 0x11
#define MARKAD_PIDTYPE_AUDIO_AC3  0x20
#define MARKAD_PIDTYPE_AUDIO_MP2  0x21

typedef struct MarkAdPid
{
    int Num;
    int Type;
} MarkAdPid;

typedef struct MarkAdContext
{
    char *LogoDir; // Logo Directory, default /var/lib/markad

    struct StandAlone
    {
        int LogoExtraction;
        int LogoWidth;
        int LogoHeight;
        MarkAdAspectRatio AspectRatioHint;
    } StandAlone;

    struct General
    {
        char *ChannelID;
        MarkAdPid VPid;
        MarkAdPid APid;
        MarkAdPid DPid;
    } General;

    struct Video
    {
        struct Options
        {
            bool IgnoreAspectRatio;
        } Options;

        struct Info
        {
            int Width;  // width of pic
            int Height; // height of pic
            int Pict_Type; // picture type (I,P,B,S,SI,SP,BI)
            MarkAdAspectRatio AspectRatio;
            double FramesPerSecond;
            bool Interlaced;
        } Info;

        struct Data
        {
            bool Valid; // flag, if true data is valid
            uchar *Plane[4];  // picture planes (YUV420)
            int PlaneLinesize[4]; // size int bytes of each picture plane line
        } Data;
    } Video;

    struct Audio
    {
        struct Info
        {
            int Channels; // number of audio channels
            int SampleRate;
        } Info;
        struct Data
        {
            bool Valid;
            short *SampleBuf;
            int SampleBufLen;
        } Data;
    } Audio;

} MarkAdContext;

#endif
