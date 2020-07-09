/**
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Description
 *     Demonstrate NDKCamera's PREVIEW mode -- hooks camera directly into
 *     display.
 *     Control:
 *         Double-Tap on Android device's screen to toggle start/stop preview
 *     Tested:
 *         Google Pixel and Nexus 6 phones
 */
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <getopt.h>
#include <string.h>
#include <termios.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <jni.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include "native_debug.h"
#include "camera_manager.h"
#include "camera_engine.h"

extern int tof_set_mode(int freq_mode,int frame_rate,int expo_time);
using namespace android;
using namespace std;
int camOpt = 0x00;
int loopStartTime = -1;
int loopStopTime = -1;

#define UNUSED(x) (void)(x)
#define MAX_CAMNUM 3

struct cam_opt {
    char id[2];
    int fps;
    int ae;
    int af;
    int s;   //saveImage
};

cam_opt cam_arg[MAX_CAMNUM] = {{"0", 30, 0, 0, 0},//default AE AF O
                      {"1", 30, 0, 0, 0},
                      {"2", 30, 0, 0, 0}};  //default 30fps

int open_count[MAX_CAMNUM] = {0, 0, 0};

CameraAppEngine *pEngineObj[MAX_CAMNUM];

static void show_usage()
{
    printf("Usage: Options:\n"
           "-h,\t\tShow this help message\n"
           "FPS AE AF S is optional, default AE AF S is 0\n"
           "[<-R>[FPS[,AE[,AF]]]],\tstart RGB camera\n"
           "[<-F>[FPS[,AE[,AF[,S]]]]] (if 1, save 10 images) ,\tstart FISHEYE camera\n"
           "[<-T>[FPS[,AE[,AF[,S]]]]] (if 1, save 10 images), \tstart TOF camera, only support FPS 10 or 30\n"
           "[<-L><loopStartTime[,loopStopTime>]], \tstart/stop camera(s) repeatly with loopTime\n");
    return;
}

void init_camera()
{
    int i;
    for (i = 0; i < MAX_CAMNUM; i++) {
        pEngineObj[i] = nullptr;
    }
}

bool check_all_camera_closed()
{
    int i;
    bool all_closed = true;
    for (i = 0; i < MAX_CAMNUM; i++) {
        if (pEngineObj[i] != nullptr) {
            all_closed = false;
            break;
        }
    }
    return all_closed;
}

void start_camera(void *arg)
{
    int i;
    cam_opt cam[MAX_CAMNUM];
    for (i = 0; i < MAX_CAMNUM; i++) {
        if (pEngineObj[i] == nullptr) {
            cam[i] = *((cam_opt*)arg+i);
            if (i == 0 && ((camOpt & 0x01) != 0x01)) {
                continue;
            }

            if (i == 1 && ((camOpt & 0x02) != 0x02)) {
                continue;
            }

            if (i == 2 && ((camOpt & 0x04) != 0x04)) {
                continue;
            }
            pEngineObj[i] = new CameraAppEngine(cam[i].id, cam[i].fps, cam[i].ae, cam[i].af, cam[i].s);
            pEngineObj[i]->CreateCameraSession();
            pEngineObj[i]->StartPreview(true);
            open_count[i] ++;
            printf("cameraId = %s, open_count[%d] = %d\n", cam[i].id, i, open_count[i]);
            usleep(1000000);
        }
    }
}

void stop_camera()
{
    int i;
    for (i = 0; i < MAX_CAMNUM; i++) {
        if (pEngineObj[i]) {
            pEngineObj[i]->StartPreview(false);
            delete pEngineObj[i];
            pEngineObj[i] = nullptr;
            printf("close cameraId = %d\n", i);
            usleep(200000);
        }
    }
}

void exit_process(int ret)
{
    int i;
    printf("--------------------------\n");
    usleep(2000000);
    printf("3camera_test end\n");
    printf("--------------------------\n");
    IPCThreadState::self()->stopProcess();
    for (i = 0; i < MAX_CAMNUM; i++) {
        open_count[i] = 0;
    }
    exit(ret);
}

void force_exit_process()
{
    printf("force exit process\n");
    exit_process(1);
}

void sig_handler(int signo)
{
    printf("%s: %d\n",__func__,signo);
    switch (signo) {
        case SIGALRM:
            if (check_all_camera_closed()) {
                start_camera((void*)cam_arg);
                alarm(loopStopTime);
            } else {
                stop_camera();
                alarm(loopStartTime);
            }
            break;
        case SIGINT:
            if (loopStartTime != -1) {
                alarm(0);
                loopStartTime = -1;
                loopStopTime = -1;
            }
            if (!check_all_camera_closed())
                stop_camera();
            exit_process(0);
            break;
        case SIGABRT:
        case SIGSEGV:
            force_exit_process();
            break;
        default:
            break;
    }
}

void juge_arg(int i){
    if(cam_arg[i].ae>=1) cam_arg[i].ae=1;
    if(cam_arg[i].ae<=0) cam_arg[i].ae=0;
    if(cam_arg[i].af>=1) cam_arg[i].af=1;
    if(cam_arg[i].af<=0) cam_arg[i].af=0;
    if(cam_arg[i].s>=1)  cam_arg[i].s=1;
    if(cam_arg[i].s<=0)  cam_arg[i].s=0;
    if(i==2){
        if(cam_arg[i].fps > 20) cam_arg[i].fps = 30;
        if(cam_arg[i].fps < 20) cam_arg[i].fps = 10;
    }else {
        if(cam_arg[i].fps > 30) cam_arg[i].fps = 30;
        if(cam_arg[i].fps < 10) cam_arg[i].fps = 10;
    }
    switch(i){
    case 0:
        printf("-R fps:%d AE:%d AF:%d S:%d\n",cam_arg[0].fps, cam_arg[0].ae,cam_arg[0].af,cam_arg[0].s);
        break;
    case 1:
        printf("-F fps:%d AE:%d AF:%d S:%d\n",cam_arg[1].fps, cam_arg[1].ae,cam_arg[1].af,cam_arg[1].s);
        break;
    case 2:
        printf("-T fps:%d AE:%d AF:%d S:%d\n",cam_arg[2].fps, cam_arg[2].ae,cam_arg[2].af,cam_arg[2].s);
        break;
    default :
        break;
    }
}
int parse_options(int argc, char **argv)
{
    char *seg2;
    char *FPS;
    char *arg[4];
    char *dot = ",";
    int dotnum=0;
    int opt=0;
    int ret = STATUS_ERROR;
    while ((opt = getopt(argc, argv, "R::r::F::f::T::t::L::l::")) != -1) {
            switch (opt) {
            case 'R':
            case 'r':
                if(optarg!=NULL){
                        seg2=optarg;
                    while(seg2!=NULL){
                        arg[dotnum++]=strsep(&seg2,dot);
                    }
                    if(arg[0]!=NULL) cam_arg[0].fps=atoi(arg[0]);
                    if(arg[1]!=NULL) cam_arg[0].ae =atoi(arg[1]);
                    if(arg[2]!=NULL) cam_arg[0].af =atoi(arg[2]);
                    if(arg[3]!=NULL) cam_arg[0].s  =atoi(arg[3]);
                }
                juge_arg(0);
                camOpt = camOpt | 0x01;
                dotnum=0;
                ret = STATUS_SUCCESS;
                break;
            case 'F':
            case 'f':
                if(optarg!=NULL){
                    seg2=optarg;
                    while(seg2!=NULL){
                        arg[dotnum++]=strsep(&seg2,dot);
                    }
                    if(arg[0]!=NULL) cam_arg[1].fps=atoi(arg[0]);
                    if(arg[1]!=NULL) cam_arg[1].ae =atoi(arg[1]);
                    if(arg[2]!=NULL) cam_arg[1].af =atoi(arg[2]);
                    if(arg[3]!=NULL) cam_arg[1].s  =atoi(arg[3]);
                }
                juge_arg(1);
                camOpt = camOpt | 0x02;
                dotnum=0;
                ret = STATUS_SUCCESS;
                break;
            case 'T':
            case 't':
                if(optarg!=NULL){
                    seg2=optarg;
                    while(seg2!=NULL){
                        arg[dotnum++]=strsep(&seg2,dot);
                    }
                    if(arg[0]!=NULL) cam_arg[2].fps=atoi(arg[0]);
                    if(arg[1]!=NULL) cam_arg[2].ae =atoi(arg[1]);
                    if(arg[2]!=NULL) cam_arg[2].af =atoi(arg[2]);
                    if(arg[3]!=NULL) cam_arg[2].s  =atoi(arg[3]);
                }
                juge_arg(2);
                camOpt = camOpt | 0x04;
                //tof set mode before open camera
                if (cam_arg[2].fps == 10) {
                    tof_set_mode(1, 10, 675); //TODO: change parameter easy to understand.
                } else if (cam_arg[2].fps == 30) {
                    tof_set_mode(0, 30, 100);
                } else {
                    LOGE("Can't support this FPS %d", cam_arg[2].fps);
                }
                dotnum=0;
                ret = STATUS_SUCCESS;
                break;
        case 'L':
        case 'l':
            if(camOpt!=0x00){
                if(optarg!=NULL) {
                    seg2 = optarg;
                    while (seg2 != NULL) {
                        arg[dotnum++] = strsep(&seg2, dot);
                    }
                    if (arg[0] != NULL) {
                        loopStartTime = atoi(arg[0]);
                        if (loopStartTime < 5) loopStartTime = 5;
                        if (arg[1] != NULL) {
                            loopStopTime = atoi(arg[1]);
                            if (loopStopTime < 3) loopStopTime = 3;
                        } else {
                            loopStopTime = loopStartTime;
                        }
                    }
                    ret = STATUS_SUCCESS;
                }
                else {
                    printf("loopTime:  +%d (should be more than 5s)\n", loopStartTime);
                    ret = STATUS_ERROR;
                }
            } else
                 ret = STATUS_ERROR;
            break;
        default:
            ret = STATUS_ERROR;
            break;
        }
    }
    return ret;
}


bool Solve_string(string& s, string& str){
    string word="";
    if(s.empty()) {
        LOGW("String is empty");
    }
    for(int i=0;i<s.length();i++){
        if(s[i]==' '){
            word+='\0';
            if(strcmp(str.c_str(),word.c_str()) == 0){
                return true;
            } //true -- screen off
            word="";
        } else
            word+=s[i];
    }
    return false; //false--screen on
}

void Read_Screen() {
    string fileName = "/sys/power/wake_unlock";
    string str = "PowerManagerService.Display";
    string s;
    int flag = 0;
    int saveloopStartTime = 0;
    int saveloopStopTime = 0;
    while(1) {
        ifstream fin(fileName.c_str());
        getline(fin,s);
        fin.close();
        if (Solve_string(s, str)) {
            if (flag == 0) {  // on->off do once
                if (loopStartTime != -1) {
                    alarm(0);
                    saveloopStartTime = loopStartTime;
                    saveloopStopTime = loopStopTime;
                    loopStartTime = -1;
                    loopStopTime = -1;
                }
                if (!check_all_camera_closed()){
                    LOGI("Screen OFF!");
                    //usleep(camOpt*100000);
                    stop_camera();
                }
                flag = 1;
            }
        } else {
            if(flag) {  // off->on do once
                LOGI("Screen ON!");
                if (check_all_camera_closed()) {
                    //usleep(camOpt*200000);
                    start_camera((void*)cam_arg);
                }
                if((saveloopStartTime != 0) && (loopStartTime == -1)) {
                    loopStartTime = saveloopStartTime;
                    loopStopTime = saveloopStopTime;
                    cout<<"saveloopStartTime:"<<saveloopStartTime<<endl;
                    saveloopStartTime = 0;
                    saveloopStopTime = 0;
                }
                if (loopStartTime != -1) {
                    alarm(1);
                }
                flag=0;
            }
        }
    }
}


int main(int argc, char *argv[])
{
    int ret, i;
    struct sigaction sa;

    if (argc < 2) {
        show_usage();
        return 1;
    }

    init_camera();

    printf("--------------------------\n");
    for(i=0; i<argc; i++)
    {
       printf("%s\n",argv[i]);
    }
    printf("--------------------------\n");

    ret = parse_options(argc, argv);
    printf("ret = %d, camOpt = %x\n ", ret, camOpt);
    if (ret != STATUS_SUCCESS) {
        show_usage();
        return STATUS_ERROR;
    }
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = &sig_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);

    if (loopStartTime != -1) {
        sigaction(SIGALRM, &sa, NULL);
        alarm(loopStopTime);    //first start_camera always execute, so trigger stop alarm.
    }

    start_camera((void*)cam_arg);
    thread ReadStatus(Read_Screen);
    IPCThreadState::self()->joinThreadPool();
    return ret;
}
