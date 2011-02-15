/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                        Intel License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000, Intel Corporation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Intel Corporation may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include "precomp.hpp"
#include <cstdio>

namespace cv
{
    
// class for grouping object candidates, detected by Cascade Classifier, HOG etc.
// instance of the class is to be passed to cv::partition (see cxoperations.hpp)
class CV_EXPORTS SimilarRects
{
public:    
    SimilarRects(double _eps) : eps(_eps) {}
    inline bool operator()(const Rect& r1, const Rect& r2) const
    {
        double delta = eps*(std::min(r1.width, r2.width) + std::min(r1.height, r2.height))*0.5;
        return std::abs(r1.x - r2.x) <= delta &&
        std::abs(r1.y - r2.y) <= delta &&
        std::abs(r1.x + r1.width - r2.x - r2.width) <= delta &&
        std::abs(r1.y + r1.height - r2.y - r2.height) <= delta;
    }
    double eps;
};    
    

static void groupRectangles(vector<Rect>& rectList, int groupThreshold, double eps, vector<int>* weights, vector<double>* foundWeights)
{
    if( groupThreshold <= 0 || rectList.empty() )
    {
        if( weights )
        {
            size_t i, sz = rectList.size();
            weights->resize(sz);
            for( i = 0; i < sz; i++ )
                (*weights)[i] = 1;
        }
        return;
    }
    
    vector<int> labels;
    int nclasses = partition(rectList, labels, SimilarRects(eps));
    
    vector<Rect> rrects(nclasses);
    vector<int> rweights(nclasses, 0);
	vector<double> outWeights(nclasses, 0.0);
    int i, j, nlabels = (int)labels.size();
    for( i = 0; i < nlabels; i++ )
    {
        int cls = labels[i];
        rrects[cls].x += rectList[i].x;
        rrects[cls].y += rectList[i].y;
        rrects[cls].width += rectList[i].width;
        rrects[cls].height += rectList[i].height;
        rweights[cls]++;
    }
	if ( foundWeights && !foundWeights->empty() )
	{
		for( i = 0; i < nlabels; i++ )
		{
			int cls = labels[i];
			outWeights[cls] = outWeights[cls] + (*foundWeights)[i];
		}
	}
    
    for( i = 0; i < nclasses; i++ )
    {
        Rect r = rrects[i];
        float s = 1.f/rweights[i];
        rrects[i] = Rect(saturate_cast<int>(r.x*s),
             saturate_cast<int>(r.y*s),
             saturate_cast<int>(r.width*s),
             saturate_cast<int>(r.height*s));
    }
    
    rectList.clear();
    if( weights )
        weights->clear();
	if( foundWeights )
		foundWeights->clear();
    
    for( i = 0; i < nclasses; i++ )
    {
        Rect r1 = rrects[i];
        int n1 = rweights[i];
		double w1 = outWeights[i];
        if( n1 <= groupThreshold )
            continue;
        // filter out small face rectangles inside large rectangles
        for( j = 0; j < nclasses; j++ )
        {
            int n2 = rweights[j];
            
            if( j == i || n2 <= groupThreshold )
                continue;
            Rect r2 = rrects[j];
            
            int dx = saturate_cast<int>( r2.width * eps );
            int dy = saturate_cast<int>( r2.height * eps );
            
            if( i != j &&
                r1.x >= r2.x - dx &&
                r1.y >= r2.y - dy &&
                r1.x + r1.width <= r2.x + r2.width + dx &&
                r1.y + r1.height <= r2.y + r2.height + dy &&
                (n2 > std::max(3, n1) || n1 < 3) )
                break;
        }
        
        if( j == nclasses )
        {
            rectList.push_back(r1);
            if( weights )
                weights->push_back(n1);
			if( foundWeights )
				foundWeights->push_back(w1);
        }
    }
}

//new grouping function with using meanshift
static void groupRectangles_meanshift(vector<Rect>& rectList, double detectThreshold, vector<double>* foundWeights, 
									  vector<double>& scales, Size winDetSize)
{
    int detectionCount = rectList.size();
    vector<Point3d> hits(detectionCount), resultHits;
    vector<double> hitWeights(detectionCount), resultWeights;
    Point2d hitCenter;

    for (int i=0; i < detectionCount; i++) 
    {
        hitWeights[i] = (*foundWeights)[i];
        hitCenter = (rectList[i].tl() + rectList[i].br())*(0.5); //center of rectangles
        hits[i] = Point3d(hitCenter.x, hitCenter.y, std::log(scales[i]));
    }

    rectList.clear();
    if (foundWeights)
        foundWeights->clear();

    double logZ = std::log(1.3);
    Point3d smothing(8, 16, logZ);

    MeanshiftGrouping msGrouping(smothing, hits, hitWeights, 1e-5, 100);

    msGrouping.getModes(resultHits, resultWeights, 1);

    for (unsigned i=0; i < resultHits.size(); ++i) 
    {

        double scale = exp(resultHits[i].z);
        hitCenter.x = resultHits[i].x;
        hitCenter.y = resultHits[i].y;
        Size s( int(winDetSize.width * scale), int(winDetSize.height * scale) );
        Rect resultRect( int(hitCenter.x-s.width/2), int(hitCenter.y-s.height/2), 
            int(s.width), int(s.height) ); 

        if (resultWeights[i] > detectThreshold) 
        {
            rectList.push_back(resultRect);
            foundWeights->push_back(resultWeights[i]);
        }
    }
}

void groupRectangles(vector<Rect>& rectList, int groupThreshold, double eps)
{
    groupRectangles(rectList, groupThreshold, eps, 0, 0);
}

void groupRectangles(vector<Rect>& rectList, vector<int>& weights, int groupThreshold, double eps)
{
    groupRectangles(rectList, groupThreshold, eps, &weights, 0);
}

void groupRectangles(vector<Rect>& rectList, vector<double>& foundWeights, int groupThreshold, double eps)
{
    groupRectangles(rectList, groupThreshold, eps, 0, &foundWeights);
}

void groupRectangles_meanshift(vector<Rect>& rectList, vector<double>& foundWeights, 
							   vector<double>& foundScales, double detectThreshold, Size winDetSize)
{
	groupRectangles_meanshift(rectList, detectThreshold, &foundWeights, foundScales, winDetSize);
}

    
#define CC_CASCADE_PARAMS "cascadeParams"
#define CC_STAGE_TYPE     "stageType"
#define CC_FEATURE_TYPE   "featureType"
#define CC_HEIGHT         "height"
#define CC_WIDTH          "width"

#define CC_STAGE_NUM    "stageNum"
#define CC_STAGES       "stages"
#define CC_STAGE_PARAMS "stageParams"

#define CC_BOOST            "BOOST"
#define CC_MAX_DEPTH        "maxDepth"
#define CC_WEAK_COUNT       "maxWeakCount"
#define CC_STAGE_THRESHOLD  "stageThreshold"
#define CC_WEAK_CLASSIFIERS "weakClassifiers"
#define CC_INTERNAL_NODES   "internalNodes"
#define CC_LEAF_VALUES      "leafValues"

#define CC_FEATURES       "features"
#define CC_FEATURE_PARAMS "featureParams"
#define CC_MAX_CAT_COUNT  "maxCatCount"

#define CC_HAAR   "HAAR"
#define CC_RECTS  "rects"
#define CC_TILTED "tilted"

#define CC_LBP  "LBP"
#define CC_RECT "rect"

#define CC_HOG  "HOG"

#define CV_SUM_PTRS( p0, p1, p2, p3, sum, rect, step )                    \
    /* (x, y) */                                                          \
    (p0) = sum + (rect).x + (step) * (rect).y,                            \
    /* (x + w, y) */                                                      \
    (p1) = sum + (rect).x + (rect).width + (step) * (rect).y,             \
    /* (x + w, y) */                                                      \
    (p2) = sum + (rect).x + (step) * ((rect).y + (rect).height),          \
    /* (x + w, y + h) */                                                  \
    (p3) = sum + (rect).x + (rect).width + (step) * ((rect).y + (rect).height)

#define CV_TILTED_PTRS( p0, p1, p2, p3, tilted, rect, step )                        \
    /* (x, y) */                                                                    \
    (p0) = tilted + (rect).x + (step) * (rect).y,                                   \
    /* (x - h, y + h) */                                                            \
    (p1) = tilted + (rect).x - (rect).height + (step) * ((rect).y + (rect).height), \
    /* (x + w, y + w) */                                                            \
    (p2) = tilted + (rect).x + (rect).width + (step) * ((rect).y + (rect).width),   \
    /* (x + w - h, y + w + h) */                                                    \
    (p3) = tilted + (rect).x + (rect).width - (rect).height                         \
           + (step) * ((rect).y + (rect).width + (rect).height)

#define CALC_SUM_(p0, p1, p2, p3, offset) \
    ((p0)[offset] - (p1)[offset] - (p2)[offset] + (p3)[offset])   
    
#define CALC_SUM(rect,offset) CALC_SUM_((rect)[0], (rect)[1], (rect)[2], (rect)[3], offset)

FeatureEvaluator::~FeatureEvaluator() {}
bool FeatureEvaluator::read(const FileNode&) {return true;}
Ptr<FeatureEvaluator> FeatureEvaluator::clone() const { return Ptr<FeatureEvaluator>(); }
int FeatureEvaluator::getFeatureType() const {return -1;}
bool FeatureEvaluator::setImage(const Mat&, Size) {return true;}
bool FeatureEvaluator::setWindow(Point) { return true; }
double FeatureEvaluator::calcOrd(int) const { return 0.; }
int FeatureEvaluator::calcCat(int) const { return 0; }

//----------------------------------------------  HaarEvaluator ---------------------------------------
class HaarEvaluator : public FeatureEvaluator
{
public:
    struct Feature
    {
        Feature();
        
        float calc( int offset ) const;
        void updatePtrs( const Mat& sum );
        bool read( const FileNode& node );
        
        bool tilted;
        
        enum { RECT_NUM = 3 };
        
        struct
        {
            Rect r;
            float weight;
        } rect[RECT_NUM];
        
        const int* p[RECT_NUM][4];
    };
    
    HaarEvaluator();
    virtual ~HaarEvaluator();

    virtual bool read( const FileNode& node );
    virtual Ptr<FeatureEvaluator> clone() const;
    virtual int getFeatureType() const { return FeatureEvaluator::HAAR; }

    virtual bool setImage(const Mat&, Size origWinSize);
    virtual bool setWindow(Point pt);

    double operator()(int featureIdx) const
    { return featuresPtr[featureIdx].calc(offset) * varianceNormFactor; }
    virtual double calcOrd(int featureIdx) const
    { return (*this)(featureIdx); }

private:
    Size origWinSize;
    Ptr<vector<Feature> > features;
    Feature* featuresPtr; // optimization
    bool hasTiltedFeatures;

    Mat sum0, sqsum0, tilted0;
    Mat sum, sqsum, tilted;
    
    Rect normrect;
    const int *p[4];
    const double *pq[4];
    
    int offset;
    double varianceNormFactor;    
};

inline HaarEvaluator::Feature :: Feature()
{
    tilted = false;
    rect[0].r = rect[1].r = rect[2].r = Rect();
    rect[0].weight = rect[1].weight = rect[2].weight = 0;
    p[0][0] = p[0][1] = p[0][2] = p[0][3] = 
        p[1][0] = p[1][1] = p[1][2] = p[1][3] = 
        p[2][0] = p[2][1] = p[2][2] = p[2][3] = 0;
}

inline float HaarEvaluator::Feature :: calc( int offset ) const
{
    float ret = rect[0].weight * CALC_SUM(p[0], offset) + rect[1].weight * CALC_SUM(p[1], offset);

    if( rect[2].weight != 0.0f )
        ret += rect[2].weight * CALC_SUM(p[2], offset);
    
    return ret;
}

inline void HaarEvaluator::Feature :: updatePtrs( const Mat& sum )
{
    const int* ptr = (const int*)sum.data;
    size_t step = sum.step/sizeof(ptr[0]);
    if (tilted)
    {
        CV_TILTED_PTRS( p[0][0], p[0][1], p[0][2], p[0][3], ptr, rect[0].r, step );
        CV_TILTED_PTRS( p[1][0], p[1][1], p[1][2], p[1][3], ptr, rect[1].r, step );
        if (rect[2].weight)
            CV_TILTED_PTRS( p[2][0], p[2][1], p[2][2], p[2][3], ptr, rect[2].r, step );
    }
    else
    {
        CV_SUM_PTRS( p[0][0], p[0][1], p[0][2], p[0][3], ptr, rect[0].r, step );
        CV_SUM_PTRS( p[1][0], p[1][1], p[1][2], p[1][3], ptr, rect[1].r, step );
        if (rect[2].weight)
            CV_SUM_PTRS( p[2][0], p[2][1], p[2][2], p[2][3], ptr, rect[2].r, step );
    }
}

bool HaarEvaluator::Feature :: read( const FileNode& node )
{
    FileNode rnode = node[CC_RECTS];
    FileNodeIterator it = rnode.begin(), it_end = rnode.end();
    
    int ri;
    for( ri = 0; ri < RECT_NUM; ri++ )
    {
        rect[ri].r = Rect();
        rect[ri].weight = 0.f;
    }
    
    for(ri = 0; it != it_end; ++it, ri++)
    {
        FileNodeIterator it2 = (*it).begin();
        it2 >> rect[ri].r.x >> rect[ri].r.y >>
            rect[ri].r.width >> rect[ri].r.height >> rect[ri].weight;
    }
    
    tilted = (int)node[CC_TILTED] != 0;
    return true;
}

HaarEvaluator::HaarEvaluator()
{
    features = new vector<Feature>();
}
HaarEvaluator::~HaarEvaluator()
{
}

bool HaarEvaluator::read(const FileNode& node)
{
    features->resize(node.size());
    featuresPtr = &(*features)[0];
    FileNodeIterator it = node.begin(), it_end = node.end();
    hasTiltedFeatures = false;
    
    for(int i = 0; it != it_end; ++it, i++)
    {
        if(!featuresPtr[i].read(*it))
            return false;
        if( featuresPtr[i].tilted )
            hasTiltedFeatures = true;
    }
    return true;
}
    
Ptr<FeatureEvaluator> HaarEvaluator::clone() const
{
    HaarEvaluator* ret = new HaarEvaluator;
    ret->origWinSize = origWinSize;
    ret->features = features;
    ret->featuresPtr = &(*ret->features)[0];
    ret->hasTiltedFeatures = hasTiltedFeatures;
    ret->sum0 = sum0, ret->sqsum0 = sqsum0, ret->tilted0 = tilted0;
    ret->sum = sum, ret->sqsum = sqsum, ret->tilted = tilted;
    ret->normrect = normrect;
    memcpy( ret->p, p, 4*sizeof(p[0]) );
    memcpy( ret->pq, pq, 4*sizeof(pq[0]) );
    ret->offset = offset;
    ret->varianceNormFactor = varianceNormFactor; 
    return ret;
}

bool HaarEvaluator::setImage( const Mat &image, Size _origWinSize )
{
    int rn = image.rows+1, cn = image.cols+1;
    origWinSize = _origWinSize;
    normrect = Rect(1, 1, origWinSize.width-2, origWinSize.height-2);
    
    if (image.cols < origWinSize.width || image.rows < origWinSize.height)
        return false;
    
    if( sum0.rows < rn || sum0.cols < cn )
    {
        sum0.create(rn, cn, CV_32S);
        sqsum0.create(rn, cn, CV_64F);
        if (hasTiltedFeatures)
            tilted0.create( rn, cn, CV_32S);
    }
    sum = Mat(rn, cn, CV_32S, sum0.data);
    sqsum = Mat(rn, cn, CV_32S, sqsum0.data);

    if( hasTiltedFeatures )
    {
        tilted = Mat(rn, cn, CV_32S, tilted0.data);
        integral(image, sum, sqsum, tilted);
    }
    else
        integral(image, sum, sqsum);
    const int* sdata = (const int*)sum.data;
    const double* sqdata = (const double*)sqsum.data;
    size_t sumStep = sum.step/sizeof(sdata[0]);
    size_t sqsumStep = sqsum.step/sizeof(sqdata[0]);
    
    CV_SUM_PTRS( p[0], p[1], p[2], p[3], sdata, normrect, sumStep );
    CV_SUM_PTRS( pq[0], pq[1], pq[2], pq[3], sqdata, normrect, sqsumStep );
    
    size_t fi, nfeatures = features->size();

    for( fi = 0; fi < nfeatures; fi++ )
        featuresPtr[fi].updatePtrs( !featuresPtr[fi].tilted ? sum : tilted );
    return true;
}

bool  HaarEvaluator::setWindow( Point pt )
{
    if( pt.x < 0 || pt.y < 0 ||
        pt.x + origWinSize.width >= sum.cols-2 ||
        pt.y + origWinSize.height >= sum.rows-2 )
        return false;

    size_t pOffset = pt.y * (sum.step/sizeof(int)) + pt.x;
    size_t pqOffset = pt.y * (sqsum.step/sizeof(double)) + pt.x;
    int valsum = CALC_SUM(p, pOffset);
    double valsqsum = CALC_SUM(pq, pqOffset);

    double nf = (double)normrect.area() * valsqsum - (double)valsum * valsum;
    if( nf > 0. )
        nf = sqrt(nf);
    else
        nf = 1.;
    varianceNormFactor = 1./nf;
    offset = (int)pOffset;

    return true;
}

//----------------------------------------------  LBPEvaluator -------------------------------------

class LBPEvaluator : public FeatureEvaluator
{
public:
    struct Feature
    {
        Feature();
        Feature( int x, int y, int _block_w, int _block_h  ) : 
        rect(x, y, _block_w, _block_h) {}
        
        int calc( int offset ) const;
        void updatePtrs( const Mat& sum );
        bool read( const FileNode& node );
        
        Rect rect; // weight and height for block
        const int* p[16]; // fast
    };
    
    LBPEvaluator();
    virtual ~LBPEvaluator();
    
    virtual bool read( const FileNode& node );
    virtual Ptr<FeatureEvaluator> clone() const;
    virtual int getFeatureType() const { return FeatureEvaluator::LBP; }

    virtual bool setImage(const Mat& image, Size _origWinSize);
    virtual bool setWindow(Point pt);
    
    int operator()(int featureIdx) const
    { return featuresPtr[featureIdx].calc(offset); }
    virtual int calcCat(int featureIdx) const
    { return (*this)(featureIdx); }
private:
    Size origWinSize;
    Ptr<vector<Feature> > features;
    Feature* featuresPtr; // optimization
    Mat sum0, sum;
    Rect normrect;

    int offset;
};    
    
    
inline LBPEvaluator::Feature :: Feature()
{
    rect = Rect();
    for( int i = 0; i < 16; i++ )
        p[i] = 0;
}

inline int LBPEvaluator::Feature :: calc( int offset ) const
{
    int cval = CALC_SUM_( p[5], p[6], p[9], p[10], offset );
    
    return (CALC_SUM_( p[0], p[1], p[4], p[5], offset ) >= cval ? 128 : 0) |   // 0
           (CALC_SUM_( p[1], p[2], p[5], p[6], offset ) >= cval ? 64 : 0) |    // 1
           (CALC_SUM_( p[2], p[3], p[6], p[7], offset ) >= cval ? 32 : 0) |    // 2
           (CALC_SUM_( p[6], p[7], p[10], p[11], offset ) >= cval ? 16 : 0) |  // 5
           (CALC_SUM_( p[10], p[11], p[14], p[15], offset ) >= cval ? 8 : 0)|  // 8
           (CALC_SUM_( p[9], p[10], p[13], p[14], offset ) >= cval ? 4 : 0)|   // 7
           (CALC_SUM_( p[8], p[9], p[12], p[13], offset ) >= cval ? 2 : 0)|    // 6
           (CALC_SUM_( p[4], p[5], p[8], p[9], offset ) >= cval ? 1 : 0);
}

inline void LBPEvaluator::Feature :: updatePtrs( const Mat& sum )
{
    const int* ptr = (const int*)sum.data;
    size_t step = sum.step/sizeof(ptr[0]);
    Rect tr = rect;
    CV_SUM_PTRS( p[0], p[1], p[4], p[5], ptr, tr, step );
    tr.x += 2*rect.width;
    CV_SUM_PTRS( p[2], p[3], p[6], p[7], ptr, tr, step );
    tr.y += 2*rect.height;
    CV_SUM_PTRS( p[10], p[11], p[14], p[15], ptr, tr, step );
    tr.x -= 2*rect.width;
    CV_SUM_PTRS( p[8], p[9], p[12], p[13], ptr, tr, step );
}

bool LBPEvaluator::Feature :: read(const FileNode& node )
{
    FileNode rnode = node[CC_RECT];
    FileNodeIterator it = rnode.begin();
    it >> rect.x >> rect.y >> rect.width >> rect.height;
    return true;
}

LBPEvaluator::LBPEvaluator()
{
    features = new vector<Feature>();
}
LBPEvaluator::~LBPEvaluator()
{
}

bool LBPEvaluator::read( const FileNode& node )
{
    features->resize(node.size());
    featuresPtr = &(*features)[0];
    FileNodeIterator it = node.begin(), it_end = node.end();
    for(int i = 0; it != it_end; ++it, i++)
    {
        if(!featuresPtr[i].read(*it))
            return false;
    }
    return true;
}

Ptr<FeatureEvaluator> LBPEvaluator::clone() const
{
    LBPEvaluator* ret = new LBPEvaluator;
    ret->origWinSize = origWinSize;
    ret->features = features;
    ret->featuresPtr = &(*ret->features)[0];
    ret->sum0 = sum0, ret->sum = sum;
    ret->normrect = normrect;
    ret->offset = offset;
    return ret;
}

bool LBPEvaluator::setImage( const Mat& image, Size _origWinSize )
{
    int rn = image.rows+1, cn = image.cols+1;
    origWinSize = _origWinSize;

    if( image.cols < origWinSize.width || image.rows < origWinSize.height )
        return false;
    
    if( sum0.rows < rn || sum0.cols < cn )
        sum0.create(rn, cn, CV_32S);
    sum = Mat(rn, cn, CV_32S, sum0.data);
    integral(image, sum);
    
    size_t fi, nfeatures = features->size();
    
    for( fi = 0; fi < nfeatures; fi++ )
        featuresPtr[fi].updatePtrs( sum );
    return true;
}
    
bool LBPEvaluator::setWindow( Point pt )
{
    if( pt.x < 0 || pt.y < 0 ||
        pt.x + origWinSize.width >= sum.cols-2 ||
        pt.y + origWinSize.height >= sum.rows-2 )
        return false;
    offset = pt.y * ((int)sum.step/sizeof(int)) + pt.x;
    return true;
}

//---------------------------------------------- HOGEvaluator -------------------------------------------

class HOGEvaluator : public FeatureEvaluator
{
public:
    struct Feature
    {
        Feature();
        //float calc( int offset, int featComponent ) const;
        float calc( int offset ) const;
        
        void updatePtrs( const vector<Mat>& _hist );
        bool read( const FileNode& node );  

        enum { CELL_NUM = 4, BIN_NUM = 9 };

        Rect rect[CELL_NUM];
        int featComponent; //component index from 0 to 35

        const float* p[CELL_NUM * BIN_NUM][4];
    };

    HOGEvaluator();
    virtual ~HOGEvaluator();

    virtual bool read( const FileNode& node );
    virtual Ptr<FeatureEvaluator> clone() const;
    virtual int getFeatureType() const { return FeatureEvaluator::HOG; }

    virtual bool setImage( const Mat& image, Size winSize );
    virtual bool setWindow( Point pt );

    double operator()(int featureIdx) const
    {
        return featuresPtr[featureIdx].calc(offset);
    }
    virtual double calcOrd( int featureIdx ) const
    {
        return (*this)(featureIdx);
    }

private:
    virtual void integralHistogram( const Mat& srcImage, vector<Mat> &histogram, int nbins ) const;

    Size origWinSize;
    Ptr<vector<Feature>> features;
    Feature* featuresPtr;
    vector<Mat> hist;

    int offset;
};

inline HOGEvaluator::Feature :: Feature()
{
    rect[0] = rect[1] = rect[2] = rect[3] = Rect();
    for( int i = 0; i < CELL_NUM * BIN_NUM; i++ )
        p[i][0] = p[i][1] = p[i][2] = p[i][3] = 0; 
    featComponent = 0;
}

float HOGEvaluator::Feature :: calc( int offset ) const
{
    int vecSize = CELL_NUM * BIN_NUM;
    float *res = new float[vecSize];
    float sum = 0.f;
    float featVal;

    for( int i = 0; i < vecSize; i++ )
    {
        res[i] = CALC_SUM(p[i], offset);
        sum += res[i];
    }
    //L1 - normalization of necessary component
    featVal = res[featComponent] / (sum + 0.001f);

    delete [] res;
    return featVal;
}

void HOGEvaluator::Feature :: updatePtrs( const vector<Mat>& _hist )
{
    const float* ptr;
    size_t step = _hist[0].step / sizeof(ptr[0]);
    int idx;
    for( int bin = 0; bin < BIN_NUM; bin++ )
    {
        ptr = (const float*)_hist[bin].data;
        for( int cell = 0; cell < CELL_NUM; cell++ )
        {
            idx = cell * BIN_NUM + bin;
            CV_SUM_PTRS( p[idx][0], p[idx][1], p[idx][2], p[idx][3], ptr, rect[idx], step );
        }
    }
}

bool HOGEvaluator::Feature :: read( const FileNode& node )
{
    FileNode rnode = node[CC_RECT];
    FileNodeIterator it = rnode.begin();
    it >> rect[0].x >> rect[0].y >> rect[0].width >> rect[0].height >> featComponent;
    rect[1].x = rect[0].x + rect[0].width;
    rect[1].y = rect[0].y;
    rect[2].x = rect[0].x;
    rect[2].y = rect[0].y + rect[0].height;
    rect[3].x = rect[0].x + rect[0].width;
    rect[3].y = rect[0].y + rect[0].height;
    rect[1].width = rect[2].width = rect[3].width = rect[0].width;
    rect[1].height = rect[2].height = rect[3].height = rect[0].height;
    return true;
}

HOGEvaluator::HOGEvaluator()
{
    features = new vector<Feature>();
}

HOGEvaluator::~HOGEvaluator()
{
}

bool HOGEvaluator::read( const FileNode& node )
{
    features->resize(node.size());
    featuresPtr = &(*features)[0];
    FileNodeIterator it = node.begin(), it_end = node.end();
    for(int i = 0; it != it_end; ++it, i++)
    {
        if(!featuresPtr[i].read(*it))
            return false;
    }
    return true;
}

Ptr<FeatureEvaluator> HOGEvaluator::clone() const
{
    HOGEvaluator* ret = new HOGEvaluator;
    ret->origWinSize = origWinSize;
    ret->features = features;
    ret->featuresPtr = &(*ret->features)[0];//??????
    ret->offset = offset;
    ret->hist = hist; //??????
    return ret;
}

bool HOGEvaluator::setImage( const Mat& image, Size winSize )
{
    int rows = image.rows + 1;
    int cols = image.cols + 1;
    origWinSize = winSize;

    if( image.cols < origWinSize.width || image.rows < origWinSize.height )
        return false;
    for( int bin = 0; bin < Feature::BIN_NUM; bin++ )
    {
        hist.push_back( Mat(rows, cols, CV_32FC1) );
    }

    integralHistogram( image, hist, Feature::BIN_NUM );

    size_t featIdx;
    size_t featCount = features->size();

    for( featIdx = 0; featIdx < featCount; featIdx++ )
    {
        featuresPtr[featIdx].updatePtrs( hist );
    }
    return true;
}

bool HOGEvaluator::setWindow(Point pt)
{
    if( pt.x < 0 || pt.y < 0 ||
        pt.x + origWinSize.width >= hist[0].cols-2 ||
        pt.y + origWinSize.height >= hist[0].rows-2 )
        return false;
    offset = pt.y * ((int)hist[0].step/sizeof(float)) + pt.x;
    return true;
}

//double HOGEvaluator::operator()(int featureIdx) const
//{
//    int _featComponent = featureIdx % (Feature::CELL_NUM * Feature::BIN_NUM);
//    return featuresPtr[featureIdx].calc(offset, _featComponent);
//}

void HOGEvaluator::integralHistogram(const Mat &srcImage, vector<Mat> &histogram, int nbins) const
{
    int x, y, ch;

    Mat src;

    Mat Dx(srcImage.rows, srcImage.cols, CV_32F);
    Mat Dy(srcImage.rows, srcImage.cols, CV_32F);
    Mat Mag(srcImage.rows, srcImage.cols, CV_32F);
    Mat Angle(srcImage.rows, srcImage.cols, CV_32F);
    Mat Bins(srcImage.rows, srcImage.cols, CV_8S);

    //Adding borders for correct gradient computation
    copyMakeBorder(srcImage, src, 1, 1, 1, 1, BORDER_REPLICATE);

    //Differential computing along both dimensions
    for (y = 1; y < src.rows - 1; y++)
    {
        for (x = 1; x < src.cols - 1; x++)
        {
            Dx.at<float>(y-1, x-1) = (float)(src.at<uchar>(y, x+1) - src.at<uchar>(y, x-1));
            Dy.at<float>(y-1, x-1) = (float)(src.at<uchar>(y+1, x) - src.at<uchar>(y-1, x));
        }
    }
    //Computing of magnitudes and angles for all vectors
    cartToPolar(Dx, Dy, Mag, Angle);

    //Angles adjusting for 9 bins
    float angleScale = (float)(nbins / CV_PI);
    float angle;
    int bidx;
    for (y = 0; y < Angle.rows; y++)
    {
        for (x = 0; x < Angle.cols; x++)
        {
            angle = Angle.at<float>(y,x)*angleScale - 0.5f;
            bidx = cvFloor(angle);

            angle -= bidx;
            if (bidx < 0)
            {
                bidx += nbins;
            }
            else if (bidx >= nbins)
            {
                bidx -= nbins;
            }
            Bins.at<char>(y,x) = (char)bidx;
        }
    }

    //Creating integral HoG
    int matIdx;
    for (ch = 0; ch < nbins; ch++)
    {
        for (int i = 0; i < histogram[ch].cols; i++)
        {
            histogram[ch].at<float>(0, i) = 0.f;
            histogram[ch].at<float>(i, 0) = 0.f;
        }
        
    }
    for (y = 1; y <= Bins.rows; y++)
    {
        Mat strSums(1, nbins, CV_32F, .0f); //����� ��������� (��������) � y-�� ������ ��� ���� 9 ������
        for (x = 1; x <= Bins.cols; x++)
        {
            matIdx = Bins.at<char>(y-1,x-1); //������ ������� � ������� ���� �������� ��������������� �������
            strSums.at<float>(matIdx) += Mag.at<float>(y-1,x-1);
            for (ch=0; ch<nbins; ch++)
            {
                float elem = histogram[ch].at<float>(y-1,x) + strSums.at<float>(ch);
                histogram[ch].at<float>(y,x) = elem;
            }
        }
    }
}

Ptr<FeatureEvaluator> FeatureEvaluator::create( int featureType )
{
    return featureType == HAAR ? Ptr<FeatureEvaluator>(new HaarEvaluator) :
        featureType == LBP ? Ptr<FeatureEvaluator>(new LBPEvaluator) : 
        featureType == HOG ? Ptr<FeatureEvaluator>(new HOGEvaluator) :
        Ptr<FeatureEvaluator>();
}            
    
//---------------------------------------- Classifier Cascade --------------------------------------------

CascadeClassifier::CascadeClassifier()
{
}

CascadeClassifier::CascadeClassifier(const string& filename)
{ load(filename); }

CascadeClassifier::~CascadeClassifier()
{
}    

bool CascadeClassifier::empty() const
{
    return oldCascade.empty() && data.stages.empty();
}

bool CascadeClassifier::load(const string& filename)
{
    oldCascade.release();
    
    FileStorage fs(filename, FileStorage::READ);
    if( !fs.isOpened() )
        return false;
    
    if( read(fs.getFirstTopLevelNode()) )
        return true;
    
    fs.release();
    
    oldCascade = Ptr<CvHaarClassifierCascade>((CvHaarClassifierCascade*)cvLoad(filename.c_str(), 0, 0, 0));
    return !oldCascade.empty();
}
    
template<class FEval>
inline int predictOrdered( CascadeClassifier& cascade, Ptr<FeatureEvaluator> &_featureEvaluator )
{
    int nstages = (int)cascade.data.stages.size();
    int nodeOfs = 0, leafOfs = 0;
    FEval& featureEvaluator = (FEval&)*_featureEvaluator;
    float* cascadeLeaves = &cascade.data.leaves[0];
    CascadeClassifier::Data::DTreeNode* cascadeNodes = &cascade.data.nodes[0];
    CascadeClassifier::Data::DTree* cascadeWeaks = &cascade.data.classifiers[0];
    CascadeClassifier::Data::Stage* cascadeStages = &cascade.data.stages[0];
    
    for( int si = 0; si < nstages; si++ )
    {
        CascadeClassifier::Data::Stage& stage = cascadeStages[si];
        int wi, ntrees = stage.ntrees;
        double sum = 0;
        
        for( wi = 0; wi < ntrees; wi++ )
        {
            CascadeClassifier::Data::DTree& weak = cascadeWeaks[stage.first + wi];
            int idx = 0, root = nodeOfs;

            do
            {
                CascadeClassifier::Data::DTreeNode& node = cascadeNodes[root + idx];
                double val = featureEvaluator(node.featureIdx);
                idx = val < node.threshold ? node.left : node.right;
            }
            while( idx > 0 );
            sum += cascadeLeaves[leafOfs - idx];
            nodeOfs += weak.nodeCount;
            leafOfs += weak.nodeCount + 1;
        }
        if( sum < stage.threshold )
            return -si;            
    }
    return 1;
}

template<class FEval>
inline int predictCategorical( CascadeClassifier& cascade, Ptr<FeatureEvaluator> &_featureEvaluator )
{
    int nstages = (int)cascade.data.stages.size();
    int nodeOfs = 0, leafOfs = 0;
    FEval& featureEvaluator = (FEval&)*_featureEvaluator;
    size_t subsetSize = (cascade.data.ncategories + 31)/32;
    int* cascadeSubsets = &cascade.data.subsets[0];
    float* cascadeLeaves = &cascade.data.leaves[0];
    CascadeClassifier::Data::DTreeNode* cascadeNodes = &cascade.data.nodes[0];
    CascadeClassifier::Data::DTree* cascadeWeaks = &cascade.data.classifiers[0];
    CascadeClassifier::Data::Stage* cascadeStages = &cascade.data.stages[0];
    
    for(int si = 0; si < nstages; si++ )
    {
        CascadeClassifier::Data::Stage& stage = cascadeStages[si];
        int wi, ntrees = stage.ntrees;
        double sum = 0;
        
        for( wi = 0; wi < ntrees; wi++ )
        {
            CascadeClassifier::Data::DTree& weak = cascadeWeaks[stage.first + wi];
            int idx = 0, root = nodeOfs;
            do
            {
                CascadeClassifier::Data::DTreeNode& node = cascadeNodes[root + idx];
                int c = featureEvaluator(node.featureIdx);
                const int* subset = &cascadeSubsets[(root + idx)*subsetSize];
                idx = (subset[c>>5] & (1 << (c & 31))) ? node.left : node.right;
            }
            while( idx > 0 );
            sum += cascadeLeaves[leafOfs - idx];
            nodeOfs += weak.nodeCount;
            leafOfs += weak.nodeCount + 1;
        }
        if( sum < stage.threshold )
            return -si;            
    }

    return 1;
}

template<class FEval>
inline int predictOrderedStump( CascadeClassifier& cascade, Ptr<FeatureEvaluator> &_featureEvaluator )
{
    int nodeOfs = 0, leafOfs = 0;
    FEval& featureEvaluator = (FEval&)*_featureEvaluator;
    float* cascadeLeaves = &cascade.data.leaves[0];
    CascadeClassifier::Data::DTreeNode* cascadeNodes = &cascade.data.nodes[0];
    CascadeClassifier::Data::Stage* cascadeStages = &cascade.data.stages[0];

    int nstages = (int)cascade.data.stages.size();
    for( int stageIdx = 0; stageIdx < nstages; stageIdx++ )
    {
        CascadeClassifier::Data::Stage& stage = cascadeStages[stageIdx];
        double sum = 0.0;

        int ntrees = stage.ntrees;
        for( int i = 0; i < ntrees; i++, nodeOfs++, leafOfs+= 2 )
        {
            CascadeClassifier::Data::DTreeNode& node = cascadeNodes[nodeOfs];
            double value = featureEvaluator(node.featureIdx);
            sum += cascadeLeaves[ value < node.threshold ? leafOfs : leafOfs + 1 ];
        }

        if( sum < stage.threshold )
            return -stageIdx;
    }

    return 1;
}

template<class FEval>
inline int predictCategoricalStump( CascadeClassifier& cascade, Ptr<FeatureEvaluator> &_featureEvaluator )
{
    int nstages = (int)cascade.data.stages.size();
    int nodeOfs = 0, leafOfs = 0;
    FEval& featureEvaluator = (FEval&)*_featureEvaluator;
    size_t subsetSize = (cascade.data.ncategories + 31)/32;
    int* cascadeSubsets = &cascade.data.subsets[0];
    float* cascadeLeaves = &cascade.data.leaves[0];
    CascadeClassifier::Data::DTreeNode* cascadeNodes = &cascade.data.nodes[0];
    CascadeClassifier::Data::Stage* cascadeStages = &cascade.data.stages[0];

    for( int si = 0; si < nstages; si++ )
    {
        CascadeClassifier::Data::Stage& stage = cascadeStages[si];
        int wi, ntrees = stage.ntrees;
        double sum = 0;

        for( wi = 0; wi < ntrees; wi++ )
        {
            CascadeClassifier::Data::DTreeNode& node = cascadeNodes[nodeOfs];
            int c = featureEvaluator(node.featureIdx);
            const int* subset = &cascadeSubsets[nodeOfs*subsetSize];
            sum += cascadeLeaves[ subset[c>>5] & (1 << (c & 31)) ? leafOfs : leafOfs+1];
            nodeOfs++;
            leafOfs += 2;
        }
        if( sum < stage.threshold )
            return -si;            
    }
    return 1;
}

int CascadeClassifier::runAt( Ptr<FeatureEvaluator>& featureEvaluator, Point pt )
{
    CV_Assert( oldCascade.empty() );
        
    assert(data.featureType == FeatureEvaluator::HAAR ||
           data.featureType == FeatureEvaluator::LBP);

    return !featureEvaluator->setWindow(pt) ? -1 :
                data.isStumpBased ? ( data.featureType == FeatureEvaluator::HAAR ?
                    predictOrderedStump<HaarEvaluator>( *this, featureEvaluator ) :
                    predictCategoricalStump<LBPEvaluator>( *this, featureEvaluator ) ) :
                                 ( data.featureType == FeatureEvaluator::HAAR ?
                    predictOrdered<HaarEvaluator>( *this, featureEvaluator ) :
                    predictCategorical<LBPEvaluator>( *this, featureEvaluator ) );
}
    
bool CascadeClassifier::setImage( Ptr<FeatureEvaluator>& featureEvaluator, const Mat& image )
{
    return empty() ? false : featureEvaluator->setImage(image, data.origWinSize);
}

struct CascadeClassifierInvoker
{
    CascadeClassifierInvoker( CascadeClassifier& _cc, Size _sz1, int _stripSize, int _yStep, double _factor, ConcurrentRectVector& _vec )
    {
        classifier = &_cc;
        processingRectSize = _sz1;
        stripSize = _stripSize;
        yStep = _yStep;
        scalingFactor = _factor;
        rectangles = &_vec;
    }
    
    void operator()(const BlockedRange& range) const
    {
        Ptr<FeatureEvaluator> evaluator = classifier->featureEvaluator->clone();
        Size winSize(cvRound(classifier->data.origWinSize.width * scalingFactor), cvRound(classifier->data.origWinSize.height * scalingFactor));

        int y1 = range.begin() * stripSize;
        int y2 = min(range.end() * stripSize, processingRectSize.height);
        for( int y = y1; y < y2; y += yStep )
        {
            for( int x = 0; x < processingRectSize.width; x += yStep )
            {
                int result = classifier->runAt(evaluator, Point(x, y));
                if( result > 0 )
                    rectangles->push_back(Rect(cvRound(x*scalingFactor), cvRound(y*scalingFactor),
                                               winSize.width, winSize.height));
                if( result == 0 )
                    x += yStep;
            }
        }
    }
    
    CascadeClassifier* classifier;
    ConcurrentRectVector* rectangles;
    Size processingRectSize;
    int stripSize, yStep;
    double scalingFactor;
};
    
struct getRect { Rect operator ()(const CvAvgComp& e) const { return e.rect; } };

bool CascadeClassifier::detectSingleScale( const Mat& image, int stripCount, Size processingRectSize,
                                           int stripSize, int yStep, double factor, vector<Rect>& candidates )
{
    if( !featureEvaluator->setImage( image, data.origWinSize ) )
        return false;

    ConcurrentRectVector concurrentCandidates;
    parallel_for(BlockedRange(0, stripCount), CascadeClassifierInvoker( *this, processingRectSize, stripSize, yStep, factor, concurrentCandidates));
    candidates.insert( candidates.end(), concurrentCandidates.begin(), concurrentCandidates.end() );

    return true;
}

bool CascadeClassifier::isOldFormatCascade() const
{
    return !oldCascade.empty();
}

int CascadeClassifier::getFeatureType() const
{
    return featureEvaluator->getFeatureType();
}

Size CascadeClassifier::getOriginalWindowSize() const
{
    return data.origWinSize;
}

bool CascadeClassifier::setImage(const Mat& image)
{
    return featureEvaluator->setImage(image, data.origWinSize);
}

void CascadeClassifier::detectMultiScale( const Mat& image, vector<Rect>& objects,
                                          double scaleFactor, int minNeighbors,
                                          int flags, Size minObjectSize, Size maxObjectSize )
{
    const double GROUP_EPS = 0.2;
    
    CV_Assert( scaleFactor > 1 && image.depth() == CV_8U );
    
    if( empty() )
        return;

    if( isOldFormatCascade() )
    {
        MemStorage storage(cvCreateMemStorage(0));
        CvMat _image = image;
        CvSeq* _objects = cvHaarDetectObjects( &_image, oldCascade, storage, scaleFactor,
                                              minNeighbors, flags, minObjectSize );
        vector<CvAvgComp> vecAvgComp;
        Seq<CvAvgComp>(_objects).copyTo(vecAvgComp);
        objects.resize(vecAvgComp.size());
        std::transform(vecAvgComp.begin(), vecAvgComp.end(), objects.begin(), getRect());
        return;
    }

    objects.clear();

    if( maxObjectSize.height == 0 || maxObjectSize.width == 0 )
        maxObjectSize = image.size();
    
    Mat grayImage = image;
    if( grayImage.channels() > 1 )
    {
        Mat temp;
        cvtColor(grayImage, temp, CV_BGR2GRAY);
        grayImage = temp;
    }
    
    Mat imageBuffer(image.rows + 1, image.cols + 1, CV_8U);
    vector<Rect> candidates;

    for( double factor = 1; ; factor *= scaleFactor )
    {
        Size originalWindowSize = getOriginalWindowSize();

        Size windowSize( cvRound(originalWindowSize.width*factor), cvRound(originalWindowSize.height*factor) );
        Size scaledImageSize( cvRound( grayImage.cols/factor ), cvRound( grayImage.rows/factor ) );
        Size processingRectSize( scaledImageSize.width - originalWindowSize.width, scaledImageSize.height - originalWindowSize.height );
        
        if( processingRectSize.width <= 0 || processingRectSize.height <= 0 )
            break;
        if( windowSize.width > maxObjectSize.width || windowSize.height > maxObjectSize.height )
            break;
        if( windowSize.width < minObjectSize.width || windowSize.height < minObjectSize.height )
            continue;
        
        Mat scaledImage( scaledImageSize, CV_8U, imageBuffer.data );
        resize( grayImage, scaledImage, scaledImageSize, 0, 0, CV_INTER_LINEAR );

        int yStep = factor > 2. ? 1 : 2;
        int stripCount, stripSize;

    #ifdef HAVE_TBB
        const int PTS_PER_THREAD = 1000;
        stripCount = ((processingRectSize.width/yStep)*(processingRectSize.height + yStep-1)/yStep + PTS_PER_THREAD/2)/PTS_PER_THREAD;
        stripCount = std::min(std::max(stripCount, 1), 100);
        stripSize = (((processingRectSize.height + stripCount - 1)/stripCount + yStep-1)/yStep)*yStep;
    #else
        stripCount = 1;
        stripSize = processingRectSize.height;
    #endif

        if( !detectSingleScale( scaledImage, stripCount, processingRectSize, stripSize, yStep, factor, candidates ) )
            break;
    }

    objects.resize(candidates.size());
    std::copy(candidates.begin(), candidates.end(), objects.begin());

    groupRectangles( objects, minNeighbors, GROUP_EPS );
}    

bool CascadeClassifier::Data::read(const FileNode &root)
{
    // load stage params
    string stageTypeStr = (string)root[CC_STAGE_TYPE];
    if( stageTypeStr == CC_BOOST )
        stageType = BOOST;
    else
        return false;

    string featureTypeStr = (string)root[CC_FEATURE_TYPE];
    if( featureTypeStr == CC_HAAR )
        featureType = FeatureEvaluator::HAAR;
    else if( featureTypeStr == CC_LBP )
        featureType = FeatureEvaluator::LBP;
    else if( featureTypeStr == CC_HOG )
        featureType = FeatureEvaluator::HOG;
    else
        return false;

    origWinSize.width = (int)root[CC_WIDTH];
    origWinSize.height = (int)root[CC_HEIGHT];
    CV_Assert( origWinSize.height > 0 && origWinSize.width > 0 );

    isStumpBased = (int)(root[CC_STAGE_PARAMS][CC_MAX_DEPTH]) == 1 ? true : false;

    // load feature params
    FileNode fn = root[CC_FEATURE_PARAMS];
    if( fn.empty() )
        return false;

    ncategories = fn[CC_MAX_CAT_COUNT];
    int subsetSize = (ncategories + 31)/32,
        nodeStep = 3 + ( ncategories > 0 ? subsetSize : 1 );

    // load stages
    fn = root[CC_STAGES];
    if( fn.empty() )
        return false;

    stages.reserve(fn.size());
    classifiers.clear();
    nodes.clear();

    FileNodeIterator it = fn.begin(), it_end = fn.end();

    for( int si = 0; it != it_end; si++, ++it )
    {
        FileNode fns = *it;
        Stage stage;
        stage.threshold = fns[CC_STAGE_THRESHOLD];
        fns = fns[CC_WEAK_CLASSIFIERS];
        if(fns.empty())
            return false;
        stage.ntrees = (int)fns.size();
        stage.first = (int)classifiers.size();
        stages.push_back(stage);
        classifiers.reserve(stages[si].first + stages[si].ntrees);

        FileNodeIterator it1 = fns.begin(), it1_end = fns.end();
        for( ; it1 != it1_end; ++it1 ) // weak trees
        {
            FileNode fnw = *it1;
            FileNode internalNodes = fnw[CC_INTERNAL_NODES];
            FileNode leafValues = fnw[CC_LEAF_VALUES];
            if( internalNodes.empty() || leafValues.empty() )
                return false;

            DTree tree;
            tree.nodeCount = (int)internalNodes.size()/nodeStep;
            classifiers.push_back(tree);

            nodes.reserve(nodes.size() + tree.nodeCount);
            leaves.reserve(leaves.size() + leafValues.size());
            if( subsetSize > 0 )
                subsets.reserve(subsets.size() + tree.nodeCount*subsetSize);

            FileNodeIterator internalNodesIter = internalNodes.begin(), internalNodesEnd = internalNodes.end();

            for( ; internalNodesIter != internalNodesEnd; ) // nodes
            {
                DTreeNode node;
                node.left = (int)*internalNodesIter; ++internalNodesIter;
                node.right = (int)*internalNodesIter; ++internalNodesIter;
                node.featureIdx = (int)*internalNodesIter; ++internalNodesIter;
                if( subsetSize > 0 )
                {
                    for( int j = 0; j < subsetSize; j++, ++internalNodesIter )
                        subsets.push_back((int)*internalNodesIter);
                    node.threshold = 0.f;
                }
                else
                {
                    node.threshold = (float)*internalNodesIter; ++internalNodesIter;
                }
                nodes.push_back(node);
            }

            internalNodesIter = leafValues.begin(), internalNodesEnd = leafValues.end();

            for( ; internalNodesIter != internalNodesEnd; ++internalNodesIter ) // leaves
                leaves.push_back((float)*internalNodesIter);
        }
    }

    return true;
}

bool CascadeClassifier::read(const FileNode& root)
{
    if( !data.read(root) )
        return false;

    // load features
    featureEvaluator = FeatureEvaluator::create(data.featureType);
    FileNode fn = root[CC_FEATURES];
    if( fn.empty() )
        return false;
    
    return featureEvaluator->read(fn);
}
    
template<> void Ptr<CvHaarClassifierCascade>::delete_obj()
{ cvReleaseHaarClassifierCascade(&obj); }    

} // namespace cv
