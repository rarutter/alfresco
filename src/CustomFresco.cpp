//CustomFresco.cpp


#include "CustomFresco.h"
#include "CustomLandscape.h"
#include "BarrenLichenMoss.h"
#include "TemperateRainforest.h"
#include "BSpruce.h"
#include "Decid.h"
#include "Tundra.h"
#include "ShrubTundra.h"
#include "GraminoidTundra.h"
#include "WetlandTundra.h"
#include "WSpruce.h"
#include "Grassland.h"
#include "Fresco.h"
#include "Landscape.h"
#include <set>


std::set<unsigned char>	CustomFresco::validVegTypes;

CustomFresco::		CustomFresco(bool isDebugOn) 
	: Fresco(new CustomLandscape(), isDebugOn)
{
}

CustomFresco::		~CustomFresco()
{
}

void CustomFresco::	customSetup()
{
	output("Loading Species settings.\n");
	gBSpruceID = (byte)fif().nGet("BSpruce");
	gWSpruceID = (byte)fif().nGet("WSpruce");
	gDecidID   = (byte)fif().nGet("Decid"); 
	gShrubTundraID  = (byte)fif().nGet("ShrubTundra");
	gGraminoidTundraID  = (byte)fif().nGet("GraminoidTundra");
	gWetlandTundraID  = (byte)fif().nGet("WetlandTundra");


	validVegTypes.insert(gBSpruceID);
	validVegTypes.insert(gWSpruceID);
	validVegTypes.insert(gDecidID);
	validVegTypes.insert(gShrubTundraID);
	validVegTypes.insert(gGraminoidTundraID);
	validVegTypes.insert(gWetlandTundraID);
	validVegTypes.insert(gNoVegID);

	// Handle Grassland here for backwards compatibility.
	if (fif().CheckKey("Grassland"))
	{
		gGrasslandID = (byte)fif().nGet("Grassland");
		validVegTypes.insert(gGrasslandID);
		Grassland::setStaticData();
	}
	if (fif().CheckKey("Tundra")) 
	{
		gTundraID = (byte)fif().nGet("Tundra");
		validVegTypes.insert(gTundraID);
		Tundra::setStaticData();
	}
	if (fif().CheckKey("BarrenLichenMoss"))
	{
		gBarrenLichenMossID = (byte)fif().nGet("BarrenLichenMoss");
		validVegTypes.insert(gBarrenLichenMossID);
	//	BarrenLichenMoss::setStaticData();
	}
	if (fif().CheckKey("TemperateRainforest"))
	{
		gTemperateRainforestID = (byte)fif().nGet("TemperateRainforest");
		validVegTypes.insert(gTemperateRainforestID);
	//	TemperateRainforest::setStaticData();
	}

	BSpruce::setStaticData();
	WSpruce::setStaticData();
	Decid::setStaticData();
	ShrubTundra::setStaticData();
	GraminoidTundra::setStaticData();
	WetlandTundra::setStaticData();
	gNumSpecies = (int)validVegTypes.size();
}
