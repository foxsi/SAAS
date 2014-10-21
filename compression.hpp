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
    int preampGain;
    int analogGain;
    int imageMinMax[2];
};

int writeFITSImage(unsigned char *data, HeaderData keys, const std::string fileName);
