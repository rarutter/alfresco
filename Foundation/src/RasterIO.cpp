//
// RasterIO.cpp
//

#include "Fresco/Foundation/PreCompiled.h"
#include "Fresco/Foundation/RasterIO.h"
#include "Fresco/Foundation/ReadRasterException.h"
#include "Fresco/Foundation/WriteRasterException.h"
#include "Fresco/Foundation/Frame.h"
#include "gdal_priv.h"
#include "cpl_conv.h" // for CPLMalloc()
#include <ogr_spatialref.h> // for OGRSpatialReference
#include <iostream>
#include <string>
#include <queue>
#include <cstdlib>
#include <math.h>
#include <limits>
#include <sstream>


using std::cout;
using std::endl;
using std::string;
using std::queue;


const byte  RasterIO::NODATA_BYTE_DEFAULT      = 255;
const int   RasterIO::NODATA_INT32_DEFAULT     = -2147483647;
const float RasterIO::NODATA_FLOAT32_DEFAULT   = -3.4e38f;
const float RasterIO::NODATA_FLOAT32_ALTERNATE = -3.40282e+38f;


RasterIO::RasterIO(int xSize, int ySize, double xOrigin, double yOrigin, double xPixelSize, 
				double yPixelSize, double xRotation, double yRotation, 
				int UTM, int isNorthernHemisphere, string coordinateSystem) 
	: _xSize(xSize),
		_ySize(ySize),
		_xOrigin(xOrigin),
		_yOrigin(yOrigin),
		_xPixelSize(xPixelSize),
		_yPixelSize(yPixelSize),
		_xRotation(xRotation),
		_yRotation(yRotation)
{
	//GDALAllRegister();
	GDALRegister_GTiff();
	GDALRegister_AAIGrid();

	// Create metadata items used for output.
	_geoTransform[0] = _xOrigin;
	_geoTransform[3] = _yOrigin;
	_geoTransform[1] = _xPixelSize;
	_geoTransform[5] = _yPixelSize;
	_geoTransform[2] = _xRotation;
	_geoTransform[4] = _yRotation;

	OGRSpatialReference sr;
	//TODO: maybe?  sr.SetProjCS( "UTM 17 (NAD83) Alaska_Albers_Equal_Area_Conic" );
	if (OGRERR_NONE != sr.SetWellKnownGeogCS(coordinateSystem.c_str()))
		throw WriteRasterException("specified coordinate system ("+coordinateSystem+") not recognized. Use a \"well known\" coordinate system such as NAD83.");
	if (OGRERR_NONE != sr.SetUTM( UTM, isNorthernHemisphere))
		throw WriteRasterException("unable to set UTM to ("+ToS(UTM)+").");

	_pSpatialReference = NULL;
	if (OGRERR_NONE != sr.exportToWkt( &_pSpatialReference ))
		throw WriteRasterException("failed to set the projection for output files using the following details: UTM=" + ToS(UTM) 
			+ ", CoordinateSystem=" + coordinateSystem + ", IsNorthernHemisphere=" + (isNorthernHemisphere ? "ture":"false"));

	_writeOptions = NULL;
	_writeOptions = CSLSetNameValue(_writeOptions, "COMPRESS", "LZW");
}


RasterIO::~RasterIO()
{
	CSLDestroy(_writeOptions);
	CPLFree(_pSpatialReference);
}


void RasterIO::getNodata(byte &result)
{
	result = RasterIO::NODATA_BYTE_DEFAULT;
}
void RasterIO::getNodata(int &result)
{
	result = RasterIO::NODATA_INT32_DEFAULT;
}
void RasterIO::getNodata(float &result)
{
	result = RasterIO::NODATA_FLOAT32_DEFAULT;
}


void RasterIO::getAlternateNodata(byte &result)
{
	result = 0; // zero used to say n/a
}
void RasterIO::getAlternateNodata(int &result)
{
	result = 0; // zero used to say n/a
}
void RasterIO::getAlternateNodata(float &result)
{
	result = RasterIO::NODATA_FLOAT32_ALTERNATE;
}


RasterIO::ALFMapType RasterIO::getMapType(int f)
{
	if (f & outVeg) return VEGEGATION;
	else if (f & outAge) return AGE;
	else if (f & outSite) return SITE_VARIABLE;
	else if (f & outSub) return SUBCANOPY;
	else if (f & outFireAge) return FIRE_AGE;
	else if (f & outFireScar) return FIRE_SCAR;
	else if (f & outfireSeverity) return BURN_SEVERITY;
	else if (f & outfireSeverityHist) return BURN_SEVERITY_HISTORY;
	else if (f & outfireSeverityHist) return BURN_SEVERITY_HISTORY;
	else if (f & out1) return DECID_SPECIES_TRAJECTORY;
	else if (f & out2) return TUNDRA_BASAL_AREA;

	std::ostringstream s;
	s << std::hex << "invalid map output flags -- " << f << " -- no data type was specified";
	throw WriteRasterException(s.str());
}


// NOTE: Could turn these zap functions into a template,
// but I think there is a bit of a performance hit taken
// in templates so avoid it when not necessary.
void RasterIO::zapRasterMemory(byte** &pMatrix)
{
	if (pMatrix != NULL) 
	{
		for (int r=0; r<_ySize; r++) 
			free(pMatrix[r]);
		free(pMatrix);
		pMatrix = NULL;
	}
}
void RasterIO::zapRasterMemory(int** &pMatrix)
{
	if (pMatrix != NULL) 
	{
		for (int r=0; r<_ySize; r++) 
			free(pMatrix[r]);
		free(pMatrix);
		pMatrix = NULL;
	}
}
void RasterIO::zapRasterMemory(float** &pMatrix)
{
	if (pMatrix != NULL) 
	{
		for (int r=0; r<_ySize; r++) 
			free(pMatrix[r]);
		free(pMatrix);
		pMatrix = NULL;
	}
}


void RasterIO::readRasterFile(const string filepath, byte**  &pMatrix, const bool isMalloc) { _readRasterFile<byte>( filepath, pMatrix, isMalloc, GDT_Byte); }
void RasterIO::readRasterFile(const string filepath, int**   &pMatrix, const bool isMalloc) { _readRasterFile<int>(  filepath, pMatrix, isMalloc, GDT_Int32); }
void RasterIO::readRasterFile(const string filepath, float** &pMatrix, const bool isMalloc) { _readRasterFile<float>(filepath, pMatrix, isMalloc, GDT_Float32); }
void RasterIO::writeRasterFile(const string filepath, Frame*** pFrames, ALFMapType mapType) 
{ 
	switch(mapType)
	{
	case VEGEGATION:
		_writeRasterFile<byte>(filepath, pFrames, mapType, GDT_Byte);
		break;
	case AGE:
		_writeRasterFile<int>(filepath, pFrames, mapType, GDT_Int32);
		break;
	case SUBCANOPY:
		_writeRasterFile<byte>(filepath, pFrames, mapType, GDT_Byte);
		break;
	case SITE_VARIABLE:
		_writeRasterFile<float>(filepath, pFrames, mapType, GDT_Float32);
		break;
	case FIRE_AGE:
		_writeRasterFile<int>(filepath, pFrames, mapType, GDT_Int32);
		break;
	case FIRE_SCAR:
		_writeRasterFile<float>(filepath, pFrames, mapType, GDT_Float32);
		break;
	case BURN_SEVERITY:
		_writeRasterFile<byte>(filepath, pFrames, mapType, GDT_Byte);
		break;
	case BURN_SEVERITY_HISTORY:
		_writeRasterFile<byte>(filepath, pFrames, mapType, GDT_Byte);
		break;
	case DECID_SPECIES_TRAJECTORY:
		_writeRasterFile<byte>(filepath, pFrames, mapType, GDT_Byte);
		break;
	case TUNDRA_BASAL_AREA:
		_writeRasterFile<float>(filepath, pFrames, mapType, GDT_Float32);
		break;
	default:
		throw WriteRasterException("undefined map type");
	}
}



//
//
//	Template for _readRasterFile<type>
//
//
template void RasterIO::_readRasterFile<byte>   (const string filepath, byte**  &pMatrix, const bool isMalloc, const GDALDataType expectedType);
template void RasterIO::_readRasterFile<int>    (const string filepath, int**   &pMatrix, const bool isMalloc, const GDALDataType expectedType);
template void RasterIO::_readRasterFile<float>  (const string filepath, float** &pMatrix, const bool isMalloc, const GDALDataType expectedType);
template<class T> void RasterIO::_readRasterFile(const string filepath, T**     &pMatrix, const bool isMalloc, const GDALDataType expectedType)
{
	T d, a; 
	getNodata(d);
	getAlternateNodata(a);
	const T defaultNodata = d;
	const T alternateNodata = a;
	
	GDALDataset* pDataset = NULL;
	try
	{
		// Get dataset	
		pDataset = (GDALDataset*) GDALOpen(filepath.c_str(), GA_ReadOnly);
		if(pDataset == NULL) 
			throw ReadRasterException("unable to open raster file at " + filepath);
		
		// Get raster band (only 1 should exist)
		int rasterCount = pDataset->GetRasterCount();
		if (rasterCount < 1 || rasterCount > 1)
			throw ReadRasterException("a total of one raster band should be available, but " + ToS(rasterCount) + " were found at " + filepath);
		GDALRasterBand* pBand = pDataset->GetRasterBand(1);
		
		// Validate
		_validateMetadata(pDataset, pBand, filepath, expectedType);
		
		int blockXSize, blockYSize;
		pBand->GetBlockSize(&blockXSize, &blockYSize);
	
		int isNoDataValid;
		const T nodata = (T)pBand->GetNoDataValue(&isNoDataValid);
		if (defaultNodata == RasterIO::NODATA_BYTE_DEFAULT)
		{
			//cast to int, otherwise value will show as char
			if (isNoDataValid)
				ShowOutput(MAXIMUM, "\t\t\tFound NoData metadata tag: " + ToS((int)nodata) + "\n");
			else
				ShowOutput(MAXIMUM, "\t\t\tWarning -- unable to find NoData metadata tag. Defaulting to " + ToS((int)defaultNodata) + "\n");
		}
		else
		{
			if (isNoDataValid)
				ShowOutput(MAXIMUM, "\t\t\tFound NoData metadata tag: " + ToS(nodata) + "\n");
			else
				ShowOutput(MAXIMUM, "\t\t\tWarning -- unable to find NoData metadata tag. Defaulting to " + ToS(defaultNodata) + "\n");
		}
	
		// check pointer
		if (isMalloc && pMatrix != NULL)
			throw ReadRasterException("logic bug -- matrix must be null before allocating memory to it. File", filepath);
		if (!isMalloc && pMatrix == NULL)
			throw ReadRasterException("logic bug -- matrix must not be null when not allocating new memory to it. File", filepath);
		
		// allocate row references
		if (isMalloc) 
			pMatrix = (T**) malloc(sizeof(T*) * _ySize);
		if (pMatrix == NULL) 
			throw ReadRasterException("unable to allocate memory needed to read raster file at " + filepath);

		CPLErr eErr = CE_None;
		for (int r=0; r < _ySize && eErr == CE_None; r++)
		{
			// allocate a row
			if (isMalloc) 
				pMatrix[r] = (T*) malloc(sizeof(T) * _xSize);	
			if (pMatrix[r] == NULL) 
				throw ReadRasterException("Unable to allocate memory needed to read raster file at " + filepath);
			
			// prepopulate with nodata
			for (int c=0; c < _xSize; c++) 
				pMatrix[r][c] =  defaultNodata;
	
			// read in a row of data
			eErr = pBand->RasterIO(GF_Read, 0, r, _xSize, 1, pMatrix[r], _xSize, 1, expectedType, 0, 0);
			
			// replace any of the file's nodata values with the default, or if metadata 
			// doesn't specify a nodata value, then replace some of the typical values used 
			if (isNoDataValid)
			{
				if (nodata != defaultNodata)
				{
					for (int c=0; c < _xSize; c++)
						if (nodata == pMatrix[r][c])
							pMatrix[r][c] = defaultNodata;
				}
			}
			else if (alternateNodata) // should only be true for float
			{
				for (int c=0; c < _xSize; c++)
					if (alternateNodata == pMatrix[r][c])
						pMatrix[r][c] = defaultNodata;
			}
		}
		
		// Clean up
		if(pDataset != NULL) 
			GDALClose((GDALDatasetH)pDataset);
		switch(eErr)
		{
			case CPLE_None:
				break;
			case CPLE_OutOfMemory:
				throw ReadRasterException("out of memory! when reading raster file at " + filepath);
				break;
			default:
				throw ReadRasterException("failed to read raster file at " + filepath);
				break; 
		}
	}
	catch(ReadRasterException& e)
	{
		if(pDataset != NULL) 
			GDALClose((GDALDatasetH)pDataset);
		zapRasterMemory(pMatrix);
		e.rethrow();
	}
	catch(...)
	{
		if (pDataset != NULL)
			GDALClose((GDALDatasetH)pDataset);
		throw ReadRasterException("unkown error while reading file at " + filepath);			
	}
}




//
//
//	Template for _writeRasterFile<type>
//
//
template void RasterIO::_writeRasterFile<byte>   (const string filepath, Frame*** pFrames, ALFMapType mapType, GDALDataType dataType);
template void RasterIO::_writeRasterFile<int>    (const string filepath, Frame*** pFrames, ALFMapType mapType, GDALDataType dataType);
template void RasterIO::_writeRasterFile<float>  (const string filepath, Frame*** pFrames, ALFMapType mapType, GDALDataType dataType);
template<class T> void RasterIO::_writeRasterFile(const string filepath, Frame*** pFrames, ALFMapType mapType, GDALDataType dataType)
{
	//TODO: validate driver in class constructor
	const char *pszFormat = "GTiff";
	GDALDriver *pDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
	if(pDriver == NULL)
		throw WriteRasterException("unable to find the GeoTIFF driver");
	
	char **pMetadata = pDriver->GetMetadata();
	if( !CSLFetchBoolean( pMetadata, GDAL_DCAP_CREATE, FALSE ) )
		throw WriteRasterException("the GeoTIFF driver that was found does not support creating new files");
	//TODO: free pMetadata? or is it owned by the driver?

	EnsureDirectoryExists(filepath, true);
	GDALDataset *pDataset = pDriver->Create(filepath.c_str(), this->_xSize, this->_ySize, 1, dataType, this->_writeOptions);
	if(pDataset == NULL)
		throw WriteRasterException("unable to create output file at " + filepath);

	T* buf;
	try
	{
		pDataset->SetGeoTransform(this->_geoTransform);
		pDataset->SetProjection(this->_pSpatialReference);
	
		GDALRasterBand *pBand = pDataset->GetRasterBand(1);
	
		T nodata;
		this->getNodata(nodata);
		if (CE_None != pBand->SetNoDataValue(nodata))
			throw WriteRasterException("unable to set the metadata tag value for NODATA");
	
		buf = (T*) malloc(this->_xSize * sizeof(T));
		if (buf == NULL) throw WriteRasterException("out of memory!");

		CPLErr err = CE_None;
		float fscar = 0.0;
		float scale = 0.0;
		for (int r=0;  r < this->_ySize  &&  err == CE_None;  r++)
		{
			for (int c=0; c<this->_xSize; c++) 
			{
				switch(mapType)
				{
				case VEGEGATION:
					buf[c] = pFrames[r][c]->type();
					break;
				case AGE:
					buf[c] = pFrames[r][c]->age();
					break;
				case SUBCANOPY:
					buf[c] = ((byte)pFrames[r][c]->speciesSubCanopy() == 0) ? (byte)pFrames[r][c]->speciesSubCanopy() : nodata;
					break;
				case SITE_VARIABLE:
					buf[c] = pFrames[r][c]->site();
					break;
				case FIRE_AGE:
					buf[c] = (pFrames[r][c]->yearOfLastBurn >= 0) ? pFrames[r][c]->yearOfLastBurn : nodata;
					break;
				case FIRE_SCAR:
					if (pFrames[r][c]->yearOfLastBurn >=0)
					{
						// format: [if ignition cell use -, otherwise +][LastBurnYear].[FireID]
						if      (pFrames[r][c]->fireScarID < 10)       scale = 0.1;
						else if (pFrames[r][c]->fireScarID < 100)      scale = 0.01;
						else if (pFrames[r][c]->fireScarID < 1000)     scale = 0.001;
						else if (pFrames[r][c]->fireScarID < 10000)    scale = 0.0001;
						else if (pFrames[r][c]->fireScarID < 100000)   scale = 0.00001;
						else if (pFrames[r][c]->fireScarID < 1000000)  scale = 0.000001;
						else if (pFrames[r][c]->fireScarID < 10000000) scale = 0.0000001;
						fscar = pFrames[r][c]->yearOfLastBurn + (pFrames[r][c]->fireScarID * scale);
						if (pFrames[r][c]->lastBurnWasOrigin) fscar *= -1;
						buf[c] = fscar;
					}
					else
					{
						buf[c] = nodata;					
					}
					break;
				case BURN_SEVERITY:
					buf[c] = (pFrames[r][c]->yearOfLastBurn==gYear ? (int)pFrames[r][c]->burnSeverity : nodata);
					break;
				case BURN_SEVERITY_HISTORY:
					buf[c] = (int)pFrames[r][c]->burnSeverity;
					break;
				case DECID_SPECIES_TRAJECTORY:
					if (pFrames[r][c]->type() == gDecidID)
						buf[c] = pFrames[r][c]->getAsByte(DECID_SPECIES_TRAJECTORY);
					else
						buf[c] = nodata;
					break;
				case TUNDRA_BASAL_AREA:
					if (pFrames[r][c]->type() == gTundraID)
						buf[c] = pFrames[r][c]->getAsFloat(TUNDRA_BASAL_AREA);
					else
						buf[c] = nodata;
					break;
				default:
					throw WriteRasterException("undefined map type");
				}
			}
			// Write one row
			err = pBand->RasterIO(GF_Write, 0, r, this->_xSize, 1, 
									buf, this->_xSize, 1, 
									dataType, 0, 0);
		}

		if (buf != NULL) free(buf);
		if (pDataset != NULL)
			GDALClose( (GDALDatasetH) pDataset );
		if (err != CE_None)
			throw WriteRasterException("failed to write to file at " + filepath);
	}
	catch(WriteRasterException& e)
	{
		if (buf != NULL) free(buf);
		if (pDataset != NULL)
			GDALClose( (GDALDatasetH) pDataset );
		e.rethrow();
	}
	catch(...)
	{
		if (buf != NULL) free(buf);
		if (pDataset != NULL)
			GDALClose( (GDALDatasetH) pDataset );
		throw WriteRasterException("unkown error while writing to file at " + filepath);			
	}
}




void RasterIO::_validateMetadata(GDALDataset* pDataset, GDALRasterBand* pBand, const std::string filepath, const GDALDataType expectedType)
{
	// Get metadata	
	double geoTransform[6];
	if (pDataset->GetGeoTransform(geoTransform) != CE_None)
		throw ReadRasterException("unable to retrieve geo transform metadata for file at " + filepath);
	double xOrigin    = geoTransform[0];
	double yOrigin    = geoTransform[3];
	double xPixelSize = geoTransform[1];
	double yPixelSize = geoTransform[5];
	double xRotation  = geoTransform[2];
	double yRotation  = geoTransform[4];
	int xRasterSize   = pDataset->GetRasterXSize();
	int yRasterSize   = pDataset->GetRasterYSize();
	int xBandSize     = pBand->GetXSize();
	int yBandSize     = pBand->GetYSize();
	GDALDataType datatype   = pBand->GetRasterDataType();	


	queue<string> errors;
	std::ostringstream s; 
	s << std::fixed;
	if (datatype != expectedType) {
		s << "expected the datatype to be " << GDALGetDataTypeName(expectedType) 
			<< " but found " << GDALGetDataTypeName(datatype); 
		errors.push(s.str()); s.str("");
	}

	if (!doubleEquals(xOrigin, _xOrigin, 0.00001)  || !doubleEquals(yOrigin, _yOrigin, 0.00001)) {
		s << "expected an origin of (" << _xOrigin << ", " <<  _yOrigin << ")"
			<< " but found (" << xOrigin << ", " <<  yOrigin << ")";
		errors.push(s.str()); s.str("");
	}

	if (!doubleEquals(xPixelSize, _xPixelSize, 0.00001)  || !doubleEquals(yPixelSize, _yPixelSize, 0.00001)) {
		s << "expected a raster pixel size of " << _xPixelSize << " x " <<  _yPixelSize
			<< " but found " << xPixelSize << " x " <<  yPixelSize;
		errors.push(s.str()); s.str("");
	}

	if (!doubleEquals(xRotation, _xRotation, 0.00001)  || !doubleEquals(yRotation, _yRotation, 0.00001)) {
		s << "expected a rotation of (" << _xRotation << ", " <<  _yRotation << ")"
			<< " but found (" << xRotation << ", " <<  yRotation << ")";
		errors.push(s.str()); s.str("");
	}

	if (xRasterSize != _xSize  || yRasterSize != _ySize) {
		s << "expected raster size to be (" << _xSize << " x " <<  _ySize << ")"
			<< " but found (" << xRasterSize << " x " <<  yRasterSize << ")";
		errors.push(s.str()); s.str("");
	}

	if (xBandSize != _xSize  || yBandSize != _ySize) {
		s << "expected raster band size to be (" << _xSize << " x " <<  _ySize << ")"
			<< " but found (" << xBandSize << " x " <<  yBandSize << ")";
		errors.push(s.str()); s.str("");
	}
	
	if (!errors.empty()) {
		int count = 1;
		while(!errors.empty()) {
			s << " (" << count << ") " << errors.front() << "; ";
			errors.pop();
			count++;
		}
		throw ReadRasterException("invalid metadata in file at " + filepath + ". The following issues were found", s.str());
	}

	//std::ios_base::fmtflags originalFormat = cout.flags();
	//cout << "METADATA" << endl;
	//cout << std::fixed << "\txOrigin\t\t" << xOrigin << endl;
	//cout << "\tyOrigin\t\t" << yOrigin << endl;    
	//cout << "\txPixelSize\t" << xPixelSize << endl; 
	//cout << "\tyPixelSize\t" << yPixelSize << endl; 
	//cout << "\txRotation\t" << xRotation << endl;  
	//cout << "\tyRotation\t" << yRotation << endl;  
	//cout << "\txRasterSize\t" << xRasterSize << endl;   
	//cout << "\tyRasterSize\t" << yRasterSize << endl;   
	//cout << "\txBandSize\t" << xBandSize << endl;     
	//cout << "\tyBandSize\t" << yBandSize << endl;
	//cout << "\tdatatype\t" << GDALGetDataTypeName(datatype) << endl;
	//cout.flags(originalFormat); // reset flags
}


void RasterIO::showLimits()
{
	using namespace std;

	cout << endl << "NUMERIC LIMITS" << endl << endl;
	cout << "    \t|\tbyte\t\tshort\t\tint\t\tfloat\t\tdouble" << endl;
	cout << "------------------------------------------------------------------------" << endl;

	cout << "min \t|\t" 
				<< (int)numeric_limits<unsigned char>::min() << "\t\t"
				<< numeric_limits<short>::min()  << "\t"
				<< numeric_limits<int>::min()  << "\t"
				<< -numeric_limits<float>::max()  << "\t"
				<< -numeric_limits<double>::max()  << "\t"
				<< endl;

	cout << "max \t|\t" 
				<< (int)numeric_limits<unsigned char>::max()  << "\t\t"
				<< numeric_limits<short>::max()  << "\t"
				<< numeric_limits<int>::max()  << "\t"
				<< numeric_limits<float>::max()  << "\t"
				<< numeric_limits<double>::max()  << "\t"
				<< endl;

	cout << "snan\t|\t" 
				<< (int)numeric_limits<unsigned char>::signaling_NaN()  << "\t\t"
				<< numeric_limits<short>::signaling_NaN()  << "\t\t"
				<< numeric_limits<int>::signaling_NaN()  << "\t\t"
				<< numeric_limits<float>::signaling_NaN()  << "\t\t"
				<< numeric_limits<double>::signaling_NaN()  << "\t\t"
				<< endl;

	cout << "qnan\t|\t" 
				<< (int)numeric_limits<unsigned char>::quiet_NaN()  << "\t\t"
				<< numeric_limits<short>::quiet_NaN()  << "\t\t"
				<< numeric_limits<int>::quiet_NaN()  << "\t\t"
				<< numeric_limits<float>::quiet_NaN()  << "\t\t"
				<< numeric_limits<double>::quiet_NaN()  << "\t\t"
				<< endl;
				
	cout << endl;
}













