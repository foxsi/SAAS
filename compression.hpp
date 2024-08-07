#include <string>
#include <ctime>

struct HeaderData
{
    timespec captureTime, captureTimeMono;
    int cameraID;
    float cameraTemperature;
    int cpuTemperature;
    long frameCount;
    int exposure;
    timespec imageWriteTime;
    float preampGain;
    int analogGain;
    int imageMinMax[2];
    float plateScale;
    int cross_x;
    int cross_y;
};

int writeFITSImage(unsigned char *data, HeaderData keys, const std::string fileName, int width, int height);
