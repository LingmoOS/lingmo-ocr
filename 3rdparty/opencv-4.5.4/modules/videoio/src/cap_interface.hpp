// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.

#ifndef CAP_INTERFACE_HPP
#define CAP_INTERFACE_HPP

#include "opencv2/core.hpp"
#include "opencv2/core/core_c.h"
#include "opencv2/videoio.hpp"
#include "opencv2/videoio/videoio_c.h"

//===================================================

// Legacy structs

struct CvCapture
{
    virtual ~CvCapture() {}
    virtual double getProperty(int) const { return 0; }
    virtual bool setProperty(int, double) { return 0; }
    virtual bool grabFrame() { return true; }
    virtual IplImage* retrieveFrame(int) { return 0; }
    virtual int getCaptureDomain() { return cv::CAP_ANY; } // Return the type of the capture object: CAP_DSHOW, etc...
};

struct CvVideoWriter
{
    virtual ~CvVideoWriter() {}
    virtual bool writeFrame(const IplImage*) { return false; }
    virtual int getCaptureDomain() const { return cv::CAP_ANY; } // Return the type of the capture object: CAP_FFMPEG, etc...
    virtual double getProperty(int) const { return 0; }
};

//===================================================

// Modern classes

namespace cv
{
namespace
{
template <class T>
inline T castParameterTo(int paramValue)
{
    return static_cast<T>(paramValue);
}

template <>
inline bool castParameterTo(int paramValue)
{
    return paramValue != 0;
}
}

class VideoParameters
{
public:
    struct VideoParameter {
        VideoParameter() = default;

        VideoParameter(int key_, int value_) : key(key_), value(value_) {}

        int key{-1};
        int value{-1};
        mutable bool isConsumed{false};
    };

    VideoParameters() = default;

    explicit VideoParameters(const std::vector<int>& params)
    {
        const auto count = params.size();
        if (count % 2 != 0)
        {
            CV_Error_(Error::StsVecLengthErr,
                      ("Vector of VideoWriter parameters should have even length"));
        }
        params_.reserve(count / 2);
        for (std::size_t i = 0; i < count; i += 2)
        {
            add(params[i], params[i + 1]);
        }
    }

    VideoParameters(int* params, unsigned n_params)
    {
        params_.reserve(n_params);
        for (unsigned i = 0; i < n_params; ++i)
        {
            add(params[2*i], params[2*i + 1]);
        }
    }

    void add(int key, int value)
    {
        params_.emplace_back(key, value);
    }

    bool has(int key) const
    {
        auto it = std::find_if(params_.begin(), params_.end(),
            [key](const VideoParameter &param)
            {
                return param.key == key;
            }
        );
        return it != params_.end();
    }

    template <class ValueType>
    ValueType get(int key) const
    {
        auto it = std::find_if(params_.begin(), params_.end(),
            [key](const VideoParameter &param)
            {
                return param.key == key;
            }
        );
        if (it != params_.end())
        {
            it->isConsumed = true;
            return castParameterTo<ValueType>(it->value);
        }
        else
        {
            CV_Error_(Error::StsBadArg, ("Missing value for parameter: [%d]", key));
        }
    }

    template <class ValueType>
    ValueType get(int key, ValueType defaultValue) const
    {
        auto it = std::find_if(params_.begin(), params_.end(),
            [key](const VideoParameter &param)
            {
                return param.key == key;
            }
        );
        if (it != params_.end())
        {
            it->isConsumed = true;
            return castParameterTo<ValueType>(it->value);
        }
        else
        {
            return defaultValue;
        }
    }

    std::vector<int> getUnused() const
    {
        std::vector<int> unusedParams;
        for (const auto &param : params_)
        {
            if (!param.isConsumed)
            {
                unusedParams.push_back(param.key);
            }
        }
        return unusedParams;
    }

    std::vector<int> getIntVector() const
    {
        std::vector<int> vint_params;
        for (const auto& param : params_)
        {
            vint_params.push_back(param.key);
            vint_params.push_back(param.value);
        }
        return vint_params;
    }

    bool empty() const
    {
        return params_.empty();
    }

    bool warnUnusedParameters() const
    {
        bool found = false;
        for (const auto &param : params_)
        {
            if (!param.isConsumed)
            {
                found = true;
                CV_LOG_INFO(NULL, "VIDEOIO: unused parameter: [" << param.key << "]=" <<
                    cv::format("%lld / 0x%016llx", (long long)param.value, (long long)param.value));
            }
        }
        return found;
    }


private:
    std::vector<VideoParameter> params_;
};

class VideoWriterParameters : public VideoParameters
{
public:
    using VideoParameters::VideoParameters;  // reuse constructors
};

class VideoCaptureParameters : public VideoParameters
{
public:
    using VideoParameters::VideoParameters;  // reuse constructors
};

class IVideoCapture
{
public:
    virtual ~IVideoCapture() {}
    virtual double getProperty(int) const { return 0; }
    virtual bool setProperty(int, double) { return false; }
    virtual bool grabFrame() = 0;
    virtual bool retrieveFrame(int, OutputArray) = 0;
    virtual bool isOpened() const = 0;
    virtual int getCaptureDomain() { return CAP_ANY; } // Return the type of the capture object: CAP_DSHOW, etc...
};

class IVideoWriter
{
public:
    virtual ~IVideoWriter() {}
    virtual double getProperty(int) const { return 0; }
    virtual bool setProperty(int, double) { return false; }
    virtual bool isOpened() const = 0;
    virtual void write(InputArray) = 0;
    virtual int getCaptureDomain() const { return cv::CAP_ANY; } // Return the type of the capture object: CAP_FFMPEG, etc...
};

namespace internal {
class VideoCapturePrivateAccessor
{
public:
    static
    IVideoCapture* getIVideoCapture(const VideoCapture& cap) { return cap.icap.get(); }
};
} // namespace

//===================================================

// Wrapper

class LegacyCapture : public IVideoCapture
{
private:
    CvCapture * cap;
    LegacyCapture(const LegacyCapture &);
    LegacyCapture& operator=(const LegacyCapture &);
public:
    LegacyCapture(CvCapture * cap_) : cap(cap_) {}
    ~LegacyCapture()
    {
        cvReleaseCapture(&cap);
    }
    double getProperty(int propId) const CV_OVERRIDE
    {
        return cap ? cap->getProperty(propId) : 0;
    }
    bool setProperty(int propId, double value) CV_OVERRIDE
    {
        return cvSetCaptureProperty(cap, propId, value) != 0;
    }
    bool grabFrame() CV_OVERRIDE
    {
        return cap ? cvGrabFrame(cap) != 0 : false;
    }
    bool retrieveFrame(int channel, OutputArray image) CV_OVERRIDE
    {
        IplImage* _img = cvRetrieveFrame(cap, channel);
        if( !_img )
        {
            image.release();
            return false;
        }
        if(_img->origin == IPL_ORIGIN_TL)
        {
            cv::cvarrToMat(_img).copyTo(image);
        }
        else
        {
            Mat temp = cv::cvarrToMat(_img);
            flip(temp, image, 0);
        }
        return true;
    }
    bool isOpened() const CV_OVERRIDE
    {
        return cap != 0;  // legacy interface doesn't support closed files
    }
    int getCaptureDomain() CV_OVERRIDE
    {
        return cap ? cap->getCaptureDomain() : 0;
    }

    CvCapture* getCvCapture() const { return cap; }
};

class LegacyWriter : public IVideoWriter
{
private:
    CvVideoWriter * writer;
    LegacyWriter(const LegacyWriter &);
    LegacyWriter& operator=(const LegacyWriter &);
public:
    LegacyWriter(CvVideoWriter * wri_) : writer(wri_)
    {}
    ~LegacyWriter()
    {
        cvReleaseVideoWriter(&writer);
    }
    double getProperty(int propId) const CV_OVERRIDE
    {
        if (writer)
        {
            return writer->getProperty(propId);
        }
        return 0.;
    }
    bool setProperty(int, double) CV_OVERRIDE
    {
        return false;
    }
    bool isOpened() const CV_OVERRIDE
    {
        return writer != NULL;
    }
    void write(InputArray image) CV_OVERRIDE
    {
        IplImage _img = cvIplImage(image.getMat());
        cvWriteFrame(writer, &_img);
    }
    int getCaptureDomain() const CV_OVERRIDE
    {
        return writer ? writer->getCaptureDomain() : 0;
    }
};

//==================================================================================================

Ptr<IVideoCapture> cvCreateFileCapture_FFMPEG_proxy(const std::string &filename, const VideoCaptureParameters& params);
Ptr<IVideoWriter> cvCreateVideoWriter_FFMPEG_proxy(const std::string& filename, int fourcc,
                                                   double fps, const Size& frameSize,
                                                   const VideoWriterParameters& params);

Ptr<IVideoCapture> createGStreamerCapture_file(const std::string& filename, const cv::VideoCaptureParameters& params);
Ptr<IVideoCapture> createGStreamerCapture_cam(int index, const cv::VideoCaptureParameters& params);
Ptr<IVideoWriter> create_GStreamer_writer(const std::string& filename, int fourcc,
                                          double fps, const Size& frameSize,
                                          const VideoWriterParameters& params);

Ptr<IVideoCapture> create_MFX_capture(const std::string &filename);
Ptr<IVideoWriter> create_MFX_writer(const std::string& filename, int _fourcc,
                                    double fps, const Size& frameSize,
                                    const VideoWriterParameters& params);

Ptr<IVideoCapture> create_AVFoundation_capture_file(const std::string &filename);
Ptr<IVideoCapture> create_AVFoundation_capture_cam(int index);
Ptr<IVideoWriter> create_AVFoundation_writer(const std::string& filename, int fourcc,
                                             double fps, const Size& frameSize,
                                             const VideoWriterParameters& params);

Ptr<IVideoCapture> create_WRT_capture(int device);

Ptr<IVideoCapture> cvCreateCapture_MSMF(int index, const VideoCaptureParameters& params);
Ptr<IVideoCapture> cvCreateCapture_MSMF(const std::string& filename, const VideoCaptureParameters& params);
Ptr<IVideoWriter> cvCreateVideoWriter_MSMF(const std::string& filename, int fourcc,
                                           double fps, const Size& frameSize,
                                           const VideoWriterParameters& params);

Ptr<IVideoCapture> create_DShow_capture(int index);

Ptr<IVideoCapture> create_V4L_capture_cam(int index);
Ptr<IVideoCapture> create_V4L_capture_file(const std::string &filename);

Ptr<IVideoCapture> create_OpenNI2_capture_cam( int index );
Ptr<IVideoCapture> create_OpenNI2_capture_file( const std::string &filename );

Ptr<IVideoCapture> create_Images_capture(const std::string &filename);
Ptr<IVideoWriter> create_Images_writer(const std::string& filename, int fourcc,
                                       double fps, const Size& frameSize,
                                       const VideoWriterParameters& params);

Ptr<IVideoCapture> create_DC1394_capture(int index);

Ptr<IVideoCapture> create_RealSense_capture(int index);

Ptr<IVideoCapture> create_PvAPI_capture( int index );

Ptr<IVideoCapture> create_XIMEA_capture_cam( int index );
Ptr<IVideoCapture> create_XIMEA_capture_file( const std::string &serialNumber );

Ptr<IVideoCapture> create_ueye_camera(int camera);

Ptr<IVideoCapture> create_Aravis_capture( int index );

Ptr<IVideoCapture> createMotionJpegCapture(const std::string& filename);
Ptr<IVideoWriter> createMotionJpegWriter(const std::string& filename, int fourcc,
                                         double fps, const Size& frameSize,
                                         const VideoWriterParameters& params);

Ptr<IVideoCapture> createGPhoto2Capture(int index);
Ptr<IVideoCapture> createGPhoto2Capture(const std::string& deviceName);

Ptr<IVideoCapture> createXINECapture(const std::string &filename);

Ptr<IVideoCapture> createAndroidCapture_cam( int index );
Ptr<IVideoCapture> createAndroidCapture_file(const std::string &filename);

bool VideoCapture_V4L_waitAny(
        const std::vector<VideoCapture>& streams,
        CV_OUT std::vector<int>& ready,
        int64 timeoutNs);

static inline
std::ostream& operator<<(std::ostream& out, const VideoAccelerationType& va_type)
{
    switch (va_type)
    {
    case VIDEO_ACCELERATION_NONE: out << "NONE"; return out;
    case VIDEO_ACCELERATION_ANY: out << "ANY"; return out;
    case VIDEO_ACCELERATION_D3D11: out << "D3D11"; return out;
    case VIDEO_ACCELERATION_VAAPI: out << "VAAPI"; return out;
    case VIDEO_ACCELERATION_MFX: out << "MFX"; return out;
    }
    out << cv::format("UNKNOWN(0x%ux)", static_cast<unsigned int>(va_type));
    return out;
}

} // cv::

#endif // CAP_INTERFACE_HPP
