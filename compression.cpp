#include "compression.hpp"
#include <CCfits>
#include <cmath>
#include <valarray>
#include <vector>

using namespace CCfits;

int writeFITSImage(unsigned char *data, HeaderData keys, const std::string fileName, int width, int height)
{
    try {

    std::string timeKey;
    if (width == 0 || height == 0)
    {
        std::cerr << "Image dimension is 0. Not saving." << std::endl;
        return -1;
    }    
    // declare auto-pointer to FITS at function scope. Ensures no resources
    // leaked if something fails in dynamic allocation.
    std::auto_ptr<FITS> pFits(0);

    try
    {                
        pFits.reset(new FITS(fileName, BYTE_IMG, 0, 0));
    }
    catch (FITS::CantCreate)
    {
        // ... or not, as the case may be.
        return -1;
    }
    catch (FITS::CantOpen)
    {
        // ... or not, as the case may be.
        std::cerr << "Could not open file to save FITS image\n";
        return -1;
    }

    long nelements(1); 

    std::vector<long int> extAx;
    extAx.push_back(width);
    extAx.push_back(height);
    string newName ("Raw Frame");

    pFits->setCompressionType(RICE_1);

    ExtHDU* imageExt;
    try{
        imageExt = pFits->addImage(newName, BYTE_IMG, extAx, 1);
    }
    catch(FitsError e){
        std::cerr << "Error while creating image extension\n";
        std::cerr << e.message() << "\n";
        return -1;
    }

    nelements = width*height*sizeof(unsigned char);

    std::valarray<unsigned char> array(data, nelements);

    long  fpixel(1);

    //add keys to the primary header
    pFits->pHDU().addKey("TELESCOP",std::string("FOXSI"),"Name of source telescope package");
//    pFits->pHDU().addKey("SIMPLE",(int)1,"always T for True, if conforming FITS file");

    pFits->pHDU().addKey("INSTRUME",std::string("SAAS"),"Name of instrument");
    pFits->pHDU().addKey("ORIGIN", std::string("FOXSI/SAAS SBC") , "Location where file was made");
    pFits->pHDU().addKey("WAVELNTH", (long)6300, "Wavelength of observation (ang)");
    pFits->pHDU().addKey("WAVE_STR", std::string("630 Nm"), "Wavelength of observation string");
    pFits->pHDU().addKey("BITPIX", (long) 8, "Bit depth of image");
    pFits->pHDU().addKey("BZERO", (long) 128, "Bit depth of image");
    pFits->pHDU().addKey("BSCALE", (float) 1.0, "Bit depth of image");
    pFits->pHDU().addKey("WAVEUNIT", std::string("angstrom"), "Units of WAVELNTH");
    pFits->pHDU().addKey("PIXLUNIT", std::string("DN"), "Pixel units");
    pFits->pHDU().addKey("IMG_TYPE", std::string("LIGHT"), "Image type");
    pFits->pHDU().addKey("RSUN_REF", (double)6.9600000e+08, "");
    pFits->pHDU().addKey("CTLMODE", std::string("LIGHT"), "Image type");
    pFits->pHDU().addKey("LVL_NUM", (int) 0 , "Level of data");

    pFits->pHDU().addKey("RSUN_OBS", (double) 0, "");

    pFits->pHDU().addKey("CTYPE1", std::string("HPLN-TAN"), "A string value labeling each coordinate axis");
    pFits->pHDU().addKey("CTYPE2", std::string("HPLN-TAN"), "A string value labeling each coordinate axis");
    pFits->pHDU().addKey("CUNIT1", std::string("arcsec"), "Coordinate Units");
    pFits->pHDU().addKey("CUNIT2", std::string("arcsec"), "Coordinate Units");
    pFits->pHDU().addKey("CRVAL1", (double)0.0, "Coordinate value of the reference pixel");
    pFits->pHDU().addKey("CRVAL2", (double)0.0, "Coordinate value of the reference pixel");
    pFits->pHDU().addKey("CDELT1", (double)keys.plateScale, "Plate scale");
    pFits->pHDU().addKey("CDELT2", (double)keys.plateScale, "Plate scale");
    pFits->pHDU().addKey("CRPIX1", (double)0.0, "Reference pixel");
    pFits->pHDU().addKey("CRPIX2", (double)0.0, "Reference pixel");

    timeKey = asctime(gmtime(&(keys.captureTime).tv_sec));
    pFits->pHDU().addKey("EXPTIME", (float)keys.exposure/1e6, "Exposure time in seconds");
    pFits->pHDU().addKey("DATE_OBS", timeKey , "Date and time when observation of this image started (UTC)");
    pFits->pHDU().addKey("TEMPCCD", (float)keys.cameraTemperature, "Temperature of camera in Celsius");

    pFits->pHDU().addKey("FILENAME", fileName , "Name of the data file");
    //pFits->pHDU().addKey("TIME", 0 , "Time of observation in seconds within a day");
    pFits->pHDU().addKey("CAMERAID", (int)keys.cameraID , "Serial Number of camera");
    pFits->pHDU().addKey("EXPOSURE", (int)keys.exposure,"Exposure time in usec");
    pFits->pHDU().addKey("GAIN_PRE", (float)keys.preampGain, "Preamp gain of CCD");
    pFits->pHDU().addKey("GAIN_ANA", (int)keys.analogGain, "Analog gain of CCD");
    pFits->pHDU().addKey("FRAMENUM", (long)keys.frameCount, "Frame number");
    

    try{
        imageExt->write(fpixel, nelements, array);
    }
    catch(FitsException e){
        std::cerr << "Exception while writing image to extension\n";
        std::cerr << e.message() << std::endl;
        return -1;
    }

    } catch (FitsError fe) {
        std::cerr << "Exception somewhere else in writeFITSImage()\n";
        std::cerr << fe.message() << std::endl;
        //throw fe;
    }

    return 0;

}