/*
 * marks.cpp: A program for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>

#include "marks.h"
extern "C" {
    #include "debug.h"
}


// global variable
extern bool abortNow;


cMark::cMark(const int typeParam, const int positionParam, const char *commentParam, const bool inBroadCastParam) {
    type = typeParam;
    position = positionParam;
    inBroadCast = inBroadCastParam;
    if (commentParam) {
        comment = strdup(commentParam);
        ALLOC(strlen(comment)+1, "comment");
    }
    else comment = NULL;

    prev = NULL;
    next = NULL;
}


cMark::~cMark() {
    if (comment) {
        FREE(strlen(comment)+1, "comment");
        free(comment);
    }
}


cMarks::cMarks() {
    strcpy(filename, "marks");
    first = last = NULL;
    count = 0;
}


cMarks::~cMarks() {
    DelAll();
}


int cMarks::Count(const int type, const int mask) {
    if (type == 0xFF) return count;
    if (!first) return 0;

    int ret = 0;
    cMark *mark = first;
    while (mark) {
        if ((mark->type & mask) == type) ret++;
        mark = mark->Next();
    }
    return ret;
}


void cMarks::Del(const int position) {
    if (!first) return; // no elements yet

    cMark *next, *mark = first;
    while (mark) {
        next = mark->Next();
        if (mark->position == position) {
            Del(mark);
            return;
        }
        mark = next;
    }
}


void cMarks::DelType(const int type, const int mask) {
    cMark *mark = first;
    while (mark) {
        if ((mark->type & mask) == type) {
            cMark *tmpMark = mark->Next();
            Del(mark);
            mark = tmpMark;
        }
        else mark = mark->Next();
    }
}


void cMarks::DelWeakFromTo(const int from, const int to, const short int type) {
    cMark *mark = first;
    while (mark) {
        if (mark->position >= to) return;
        if ((mark->position > from) && (mark->type < (type & 0xF0))) {
            cMark *tmpMark = mark->Next();
            Del(mark);
            mark = tmpMark;
        }
        else mark = mark->Next();
    }
}


// delete all marks <from> <to> of <type>
// include <from> and <to>
//
void cMarks::DelFromTo(const int from, const int to, const short int type) {
    cMark *mark = first;
    while (mark) {
        if (mark->position > to) return;
        if ((mark->position >= from) && ((mark->type & 0xF0) == type)) {
            cMark *tmpMark = mark->Next();
            Del(mark);
            mark = tmpMark;
        }
        else mark = mark->Next();
    }
}


// <FromStart> = true: delete all marks from start to <Position>
// <FromStart> = false: delete all marks from <Position> to end
//
void cMarks::DelTill(const int position, const bool fromStart) {
    cMark *next, *mark = first;
    if (!fromStart) {
        while (mark) {
            if (mark->position > position) break;
            mark = mark->Next();
        }
    }
    while (mark) {
        next = mark->Next();
        if (fromStart) {
            if (mark->position < position) {
                Del(mark);
            }
        }
        else {
            Del(mark);
        }
        mark = next;
    }
}


void cMarks::DelFrom(const int position) {
    cMark *mark = first;
    while (mark) {
        if (mark->position > position) break;
        mark = mark->Next();
    }

    while (mark) {
        cMark * next = mark->Next();
        Del(mark);
        mark = next;
    }
}


void cMarks::DelAll() {
    cMark *next, *mark = first;
    while (mark) {
        next = mark->Next();
        Del(mark);
        mark=next;
    }
    first = NULL;
    last = NULL;
}


void cMarks::DelInvalidSequence() {
    cMark *mark = first;
    while (mark) {

        // if first mark is a stop mark, remove it
        if (((mark->type & 0x0F) == MT_STOP) && (mark == first)){
            dsyslog("cMarks::DelInvalidSequence(): Start with STOP mark (%d) type %d, delete first mark", mark->position, mark->type);
            cMark *tmp = mark;
            mark = mark->Next();
            Del(tmp);
            continue;
        }

        // cleanup of start followed by start or stop followed by stop
        if ((((mark->type & 0x0F) == MT_STOP)  && (mark->Next()) && ((mark->Next()->type & 0x0F) == MT_STOP)) || // two stop or start marks, keep strong marks, delete weak
            (((mark->type & 0x0F) == MT_START) && (mark->Next()) && ((mark->Next()->type & 0x0F) == MT_START))) {
            dsyslog("cMarks::DelInvalidSequence(): mark (%d) type %d, followed by same mark (%d) type %d", mark->position, mark->type, mark->Next()->position, mark->Next()->type);
            if (mark->type < mark->Next()->type) {
                dsyslog(" cMarks::DelInvalidSequence() delete mark (%d)", mark->position);
                cMark *tmp = mark;
                mark = mark->Next();
                Del(tmp);
                continue;
            }
            else {
                dsyslog(" cMarks::DelInvalidSequence() delete mark (%d)", mark->Next()->position);
                Del(mark->Next());
            }
        }
        mark = mark->Next();
    }
}



void cMarks::Del(cMark *mark) {
    if (!mark) return;

    if (first == mark) {
        // we are the first mark
        first = mark->Next();
        if (first) {
            first->SetPrev(NULL);
        }
        else {
            last = NULL;
        }
    }
    else {
        if (mark->Next() && (mark->Prev())) {
            // there is a next and prev object
            mark->Prev()->SetNext(mark->Next());
            mark->Next()->SetPrev(mark->Prev());
        }
        else {
            // we are the last
            mark->Prev()->SetNext(NULL);
            last=mark->Prev();
        }
    }
    FREE(sizeof(*mark), "mark");
    delete mark;
    count--;
}


cMark *cMarks::Get(const int position) {
    if (!first) return NULL; // no elements yet

    cMark *mark = first;
    while (mark) {
        if (position == mark->position) break;
        mark = mark->Next();
    }
    return mark;
}


cMark *cMarks::GetAround(const int frames, const int position, const int type, const int mask) {
    cMark *m0 = Get(position);
    if (m0 && (m0->position == position) && ((m0->type & mask) == (type & mask))) return m0;

    cMark *m1 = GetPrev(position, type, mask);
    cMark *m2 = GetNext(position, type, mask);
    if (!m1 && !m2) return NULL;

    if (!m1 && m2) {
        if (abs(position - m2->position) > frames) return NULL;
        else return m2;
    }
    if (m1 && !m2) {
        if (abs(position - m1->position) > frames) return NULL;
        return m1;
    }
    if (m1 && m2) {
        if (abs(m1->position - position) > abs(m2->position - position)) {
            if (abs(position - m2->position) > frames) return NULL;
            else return m2;
        }
        else {
            if (abs(position - m1->position) > frames) return NULL;
            return m1;
        }
    }
    else {
        dsyslog("cMarks::GetAround(): invalid marks found");
        return NULL;
    }
}


cMark *cMarks::GetPrev(const int position, const int type, const int mask) {
    if (!first) return NULL; // no elements yet

    // first advance
    cMark *mark = first;
    while (mark) {
        if (mark->position >= position) break;
        mark = mark->Next();
    }
    if (type == 0xFF) {
        if (mark) return mark->Prev();
        return last;
    }
    else {
        if (!mark) mark = last;
        else mark = mark->Prev();
        while (mark) {
            if ((mark->type & mask) == type) break;
            mark = mark->Prev();
        }
        return mark;
    }
}


cMark *cMarks::GetNext(const int position, const int type, const int mask) {
    if (!first) return NULL; // no elements yet
    cMark *mark = first;
    while (mark) {
        if (type == 0xFF) {
            if (mark->position > position) break;
        }
        else {
            if ((mark->position > position) && ((mark->type & mask) == type)) break;
        }
        mark = mark->Next();
    }
    if (mark) return mark;
    return NULL;
}


cMark *cMarks::Add(const int type, const int position, const char *comment, const bool inBroadCast) {

    cMark *dupMark;
    if ((dupMark = Get(position))) {
        dsyslog("cMarks::Add(): duplicate mark on position %i type 0x%X and type 0x%X", position, type, dupMark->type);
        if (type == dupMark->type) return dupMark;      // same type at same position, ignore add
        if ((type & 0xF0) == (dupMark->type & 0xF0)) {  // start and stop mark of same type at same position, delete both
            Del(dupMark->position);
            return NULL;
        }
        if (type > dupMark->type){   // keep the stronger mark
            if (dupMark->comment && comment) {
                FREE(strlen(dupMark->comment)+1, "comment");
                free(dupMark->comment);
                dupMark->comment = strdup(comment);
                ALLOC(strlen(dupMark->comment)+1, "comment");
            }
            dupMark->type = type;
            dupMark->inBroadCast = inBroadCast;
        }
        return dupMark;
    }

    cMark *newMark = new cMark(type, position, comment, inBroadCast);
    if (!newMark) return NULL;
    ALLOC(sizeof(*newMark), "mark");

    if (!first) {
        //first element
        first = last = newMark;
        count++;
        return newMark;
    }
    else {
        cMark *mark = first;
        while (mark) {
            if (!mark->Next()) {
                if (position > mark->position) {
                    // add as last element
                    newMark->Set(mark, NULL);
                    mark->SetNext(newMark);
                    last = newMark;
                    break;
                }
                else {
                    // add before
                    if (!mark->Prev()) {
                        // add as first element
                        newMark->Set(NULL, mark);
                        mark->SetPrev(newMark);
                        first = newMark;
                        break;
                    }
                    else {
                        newMark->Set(mark->Prev(), mark);
                        mark->SetPrev(newMark);
                        break;
                    }
                }
            }
            else {
                if ((position > mark->position) && (position < mark->Next()->position)) {
                    // add between two marks
                    newMark->Set(mark, mark->Next());
                    mark->SetNext(newMark);
                    newMark->Next()->SetPrev(newMark);
                    break;
                }
                else {
                    if ((position < mark->position) && (mark == first)) {
                        // add as first mark
                        first = newMark;
                        mark->SetPrev(newMark);
                        newMark->SetNext(mark);
                        break;
                    }
                }
            }
            mark = mark->Next();
        }
        if (!mark)return NULL;
        count++;
        return newMark;
    }
    return NULL;
}


// define text to mark type, used in marks file and log messages
// return pointer to text, calling function has to free memory
//
char *cMarks::TypeToText(const int type) {
    char *text = NULL;
    switch (type & 0xF0) {
        case MT_ASSUMED:
            if (asprintf(&text, "assumed") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        case MT_BLACKCHANGE:
            if (asprintf(&text, "black screen") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        case MT_LOGOCHANGE:
            if (asprintf(&text, "logo") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        case MT_CHANNELCHANGE:
            if (asprintf(&text, "channel") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        case MT_VBORDERCHANGE:
            if (asprintf(&text, "vertical border") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        case MT_HBORDERCHANGE:
            if (asprintf(&text, "horizontal border") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        case MT_ASPECTCHANGE:
            if (asprintf(&text, "aspect ratio") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        case MT_MOVEDCHANGE:
            if (asprintf(&text, "moved") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        case MT_RECORDINGCHANGE:
            if (asprintf(&text, "recording") != -1) {
                ALLOC(strlen(text)+1, "text");
            }
            break;
        default:
           if (asprintf(&text, "unknown") != -1) {
               ALLOC(strlen(text)+1, "text");
           }
           break;
    }
    return text;
}


// move mark to new posiition
// return pointer to new mark
//
cMark *cMarks::Move(cMark *mark, const int newPosition, const char* reason) {
    if (!mark) return NULL;
    if (!reason) return NULL;

    char *comment = NULL;
    char *indexToHMSF = IndexToHMSF(mark->position);
    cMark *newMark = NULL;
    char* typeText = TypeToText(mark->type);

    if (indexToHMSF && typeText) {
       if (asprintf(&comment,"moved %s mark (%6d) %s %-5s %s mark (%6d) at %s, %s detected%s",
                                    ((mark->type & 0x0F) == MT_START) ? "start" : "stop ",
                                             newPosition,
                                                  (newPosition > mark->position) ? "after " : "before",
                                                     typeText,
                                                        ((mark->type & 0x0F) == MT_START) ? "start" : "stop ",
                                                                 mark->position,
                                                                       indexToHMSF,
                                                                           reason,
                                                                                      ((mark->type & 0x0F) == MT_START) ? "*" : "") != -1) {
           ALLOC(strlen(comment)+1, "comment");
           isyslog("%s",comment);

           int newType = ((mark->type & 0x0F) == MT_START) ? MT_MOVEDSTART : MT_MOVEDSTOP;
           Del(mark);
           newMark = Add(newType, newPosition, comment);

           FREE(strlen(typeText)+1, "text");
           free(typeText);
           FREE(strlen(comment)+1, "comment");
           free(comment);
           FREE(strlen(indexToHMSF)+1, "indexToHMSF");
           free(indexToHMSF);
       }
   }
   return newMark;
}


void cMarks::RegisterIndex(cIndex *recordingIndex) {
    recordingIndexMarks = recordingIndex;
}


char *cMarks::IndexToHMSF(const int frameNumber, const bool isVDR) {
    char *indexToHMSF = NULL;
    double Seconds = 0;
    int f = 0;
    int time_ms = recordingIndexMarks->GetTimeFromFrame(frameNumber, isVDR);
    if (time_ms >= 0) f = int(modf(float(time_ms) / 1000, &Seconds) * 100);                 // convert ms to 1/100 s
    else {
        dsyslog("cMarks::IndexToHMSF(): failed to get time from frame (%d)", frameNumber);
        return NULL;
    }
    int s = int(Seconds);
    int m = s / 60 % 60;
    int h = s / 3600;
    s %= 60;
    if (asprintf(&indexToHMSF, "%d:%02d:%02d.%02d", h, m, s, f) == -1) return NULL;   // this has to be freed in the calling function
    ALLOC(strlen(indexToHMSF)+1, "indexToHMSF");
    return indexToHMSF;
}


bool cMarks::Backup(const char *directory) {
    char *fpath = NULL;
    if (asprintf(&fpath, "%s/%s", directory, filename) == -1) return false;
    ALLOC(strlen(fpath)+1, "fpath");

    // make backup of old marks, filename convention taken from noad
    char *bpath = NULL;
    if (asprintf(&bpath, "%s/%s0", directory, filename) == -1) {
        FREE(strlen(fpath)+1, "fpath");
        free(fpath);
        return false;
    }
    ALLOC(strlen(bpath)+1, "bpath");

    int ret = rename(fpath,bpath);
    FREE(strlen(bpath)+1, "bpath");
    free(bpath);
    FREE(strlen(fpath)+1, "fpath");
    free(fpath);
    return (ret == 0);
}


int cMarks::LoadVPS(const char *directory, const char *type) {
    if (!directory) return -1;
    if (!type) return -1;

    char *fpath = NULL;
    if (asprintf(&fpath, "%s/%s", directory, "markad.vps") == -1) return -1;
    FILE *mf;
    mf = fopen(fpath, "r+");
    if (!mf) {
        dsyslog("cMarks::LoadVPS(): %s not found", fpath);
        free(fpath);
        return -1;
    }
    free(fpath);

    char *line = NULL;
    size_t length = 0;
    char typeVPS[16] = "";
    char timeVPS[21] = "";
    int offsetVPS = 0;
    while (getline(&line, &length,mf) != -1) {
        sscanf(line, "%15s %20s %d", (char *) &typeVPS, (char *)&timeVPS, &offsetVPS);
        if (strcmp(type, typeVPS) == 0) break;
        offsetVPS = -1;
    }
    if (line) free(line);
    fclose(mf);
    return offsetVPS;
}


bool cMarks::Save(const char *directory, const sMarkAdContext *maContext, const bool force) {
    if (!directory) return false;
    if (!maContext) return false;
    if (!first) return false;  // no marks to save
    if (abortNow) return false;  // do not save marks if aborted

    if (!maContext->Info.isRunningRecording && !force) {
//        dsyslog("cMarks::Save(): save marks later, isRunningRecording=%d force=%d", maContext->Info.isRunningRecording, force);
        return false;
    }
    dsyslog("cMarks::Save(): save marks, isRunningRecording=%d force=%d", maContext->Info.isRunningRecording, force);

    char *fpath = NULL;
    if (asprintf(&fpath, "%s/%s", directory, filename) == -1) return false;
    ALLOC(strlen(fpath)+1, "fpath");

    FILE *mf;
    mf = fopen(fpath,"w+");

    if (!mf) {
        FREE(strlen(fpath)+1, "fpath");
        free(fpath);
        return false;
    }

    cMark *mark = first;
    while (mark) {
        if (((mark->type & 0xF0) == MT_BLACKCHANGE) && (mark->position != first->position) && (mark->position != last->position)) { // do not save blackscreen marks expect start and end mark
            mark = mark->Next();
            continue;
        }
        // for stop marks adjust timestamp from iFrame before to prevent short ad pictures, vdr cut only on iFrames
        int vdrMarkPosition = mark->position;
        if ((mark->type & 0x0F) == MT_STOP) vdrMarkPosition = recordingIndexMarks->GetIFrameBefore(mark->position);

        char *indexToHMSF_VDR = IndexToHMSF(vdrMarkPosition, true);
        char *indexToHMSF_PTS = IndexToHMSF(mark->position, false);
        if (indexToHMSF_VDR && indexToHMSF_PTS) {
            if (maContext->Config->pts) fprintf(mf, "%s (%6d)%s %s <- %s\n", indexToHMSF_VDR, mark->position, ((mark->type & 0x0F) == MT_START) ? "*" : " ", indexToHMSF_PTS, mark->comment ? mark->comment : "");
            else fprintf(mf, "%s (%6d)%s %s\n", indexToHMSF_VDR, mark->position, ((mark->type & 0x0F) == MT_START) ? "*" : " ", mark->comment ? mark->comment : "");
            FREE(strlen(indexToHMSF_VDR)+1, "indexToHMSF");
            free(indexToHMSF_VDR);
            FREE(strlen(indexToHMSF_PTS)+1, "indexToHMSF");
            free(indexToHMSF_PTS);
        }
        mark = mark->Next();
    }
    fclose(mf);

    if (geteuid() == 0) {
        // if we are root, set fileowner to owner of 001.vdr/00001.ts file
        char *spath = NULL;
        if (asprintf(&spath, "%s/00001.ts", directory) != -1) {
            ALLOC(strlen(spath)+1, "spath");
            struct stat statbuf;
            if (!stat(spath, &statbuf)) {
                if (chown(fpath, statbuf.st_uid, statbuf.st_gid)) {};
            }
            FREE(strlen(spath)+1, "spath");
            free(spath);
        }
    }
    FREE(strlen(fpath)+1, "fpath");
    free(fpath);
    return true;
}
