#ifndef Fresco_CustomLandscape_h
#define Fresco_CustomLandscape_h


#include "Fresco/Custom/Custom.h"
#include "Fresco/Foundation/Landscape.h"
//Include frame types
#include "Tundra.h"
#include "BSpruce.h"
#include "WSpruce.h"
#include "Decid.h"


class FrescoCustom_API CustomLandscape : public Landscape 
{
//Data
public:
	enum LayerType		{FIRE,FIREAGE,FIRESUPPRESSION,AGE,VEGETATION,SUBCANOPY,SITE};
private:
    struct SSuppressionTransition
    {
	    int			Year;
	    bool        HasNewMap;
	    float		Classes[6];
	    int         ThresholdFireSize;
        int         ThresholdIgnitions;
	    SSuppressionTransition() : Year(0), HasNewMap(false), ThresholdFireSize(0), ThresholdIgnitions(0) {};
    };
    std::vector<SSuppressionTransition>  _suppressionTransitions;
    struct SMapStat
    {
	    std::string	File;
	    std::string	Code;
	    int		    Flags;
	    int		    repStart;
	    int		    RepFreq;
	    int		    yearStart;
	    int		    YearFreq;
	    SMapStat() : File(""), Code(""), Flags(0), repStart(0), RepFreq(0), yearStart(0), YearFreq(0) {};
    };
    std::vector<SMapStat> _mapStats;
    struct SHabitatStat
    {
        CStat Stat;                                 //Used to tally cells that meet the habitat's criteria.
        std::vector<int> VegTypes;                  //A list of veg types that meet the habitat's criteria.
        int MinAge;                                 //Minimum age a cell can have to meet the habitat's criteria.
        int MaxAge;                                 //Minimum age a cell can have to meet the habitat's criteria.
        SHabitatStat() : MinAge(0), MaxAge(0) {};
    };
    std::vector<SHabitatStat> _habitatStats;
	//Veg Layer
	std::string			_vegInputFile;
	int**				_pVegSpatialInput;
	bool				_isForcedVegTransitions;
	std::string			_vegTransitionFile;
	bool				_isUsingUniqueVegAndAgePerRep;
    int					_yearOfVegAndAgeFiles;
	//Age Layer
	std::string			_ageInputFile;
	int**				_pAgeSpatialInput;
	//Topography Layer
	std::string			_topoInputFile;
	int**				_pTopoSpatialInput;
	//Site Layer
	std::string			_siteInputFile;
	float**				_pSiteSpatialInput;
	//Tree Density Layer
	std::string			_treeDensityInputFile;
	int**				_pTreeDensitySpatialInput;
	//Burn Severity Layer
	std::string			_burnSeverityInputFile;
	int**				_pBurnSeveritySpatialInput;
	//Fire Layer
	int**				_pHistoricalFireSpatialInput;
	float**				_pIgnitionFactorSpatialInput;
	float**				_pSensitivitySpatialInput;
	int**				_pSuppressions;											//Suppression Map values 0-5 where 0 is always represents no suppression.
    static const int    NUM_SUPPRESSION_CLASSES = 5;
	bool			    _isFireSuppressionOn;
    std::string	        _suppressionFilename;
	float				_pSuppressionClasses[6];								//Suppression class values.
	int			        _thresholdFireSize;										//The cutting point where suppressions stop being applied, because the sum of the year's fire sizes exceeds the capability of available suppression resources. 
	int			        _thresholdIgnitions;									//The cutting point where suppressions stop being applied, because the amount of ignitions for the year exceeds the capability of available suppression resources.
    std::vector<CStat>  _burnPartitionBySuppClassStats;	                        //Number of burnt cells within each suppression class.
    long                _burnPartitionBySuppClassStatFlags;

//Functions
public:
						CustomLandscape(const int rows=-1, const int cols=-1);
	virtual 			~CustomLandscape();

    virtual void		clear(); 
	virtual void		setup();
	virtual void		repStart(); 
	virtual void		yearStart();
	virtual void		yearEnd();
	
	virtual void		doIgnitions();				
    void				doFireTransitions();
	void				doVegetationTransitions();
    void                doFireSuppressionTransitions();
    void                writeMaps();
    void				fillArray(LayerType layerType, std::vector< std::vector<double> >& rArray);
protected:
    virtual const float getCustomFireSpreadMultiplier(const unsigned int row, const unsigned int col, const unsigned int fireSizeTotal, const unsigned int fireNum);
private:
    void                clearLayers();
    const float         getCellFireSuppression(const unsigned int row, const unsigned int col, const unsigned int fireSizeTotal, const unsigned int fireNum);
    void                setupSuppressionStats();
    void                setupHabitatStats();
    void                setupMapStats();
    void                collectStats();
	//Suppression Transitions
    void                setupSuppressionTransitions();
	void				setCurrentSuppressionTransition(std::vector<SSuppressionTransition>::iterator transition);
	void				applySuppressionTransitionIfExists(int year);
};


#endif
