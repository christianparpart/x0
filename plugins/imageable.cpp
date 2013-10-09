/* <x0/plugins/imageresizer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

/**
 * @file imageable.cpp
 *
 * This Plugin is to resize, fit and crop images.
 *
 * These images can either be located directly on local disk or passed as a remote URL to fetch.
 *
 * Feature Goals:
 * <ul>
 *   <li>1:1 API compatibility to node-imageable service (nodejs application)
 *   <li>respect If-Modified-Since and ETag
 *   <li>cache resized iamge witha ttl
 *   <li>allow fetching source image from local disk as well as remote HTTP URLs (GET request)
 *   <li>security: query arg "key" to be xor'd against a PSK that must result into a timestamp that may not differ more than N seconds to be accepted.
 *   <li>image processing outside http workers in dedicated worker cluster.
 * </ul>
 *
 * Flow API:
 * <code>
 *   // number of processing worker threads, just like `workers` but for image processing
 *   property int imageable.workers = 1;
 *
 *   // TTL until a cached image gets deleted on local cache, 0 means disabled
 *   property timespan imageable.ttl = 0;
 *
 *   // cache temp directory
 *   property string imageable.tempdir = '/var/tmp/x0/imageable';
 *
 *   // handler to actually serve the processed image
 *   handler imageable();
 * </code>
 *
 * HTTP API:
 *
 * Image processing actions are passed as URL query arguments.
 * <code>
 *  ACTION ::= RESIZE_ACTION | CROP_ACTION | FIT_ACTION
 * 	RESIZE_ACTION ::= '/resize/' MAGIC_HASH [PARAMS]
 * 	CROP_ACTION ::= '/crop/' MAGIC_HASH [PARAMS]
 * 	FIT_ACTION ::= '/fit/' MAGIC_HASH [PARAMS]
 *
 * 	PARAMS ::= '?' PARAM ['&' PARAM]*
 * 	URL  ::= 'http://' hostname [':' PORT] PATH
 * 	SIZE ::= WIDTH 'x' HEIGHT
 * 	CROP ::= WIDTH 'x' HEIGHT ['+' X '+' Y] | 'true'
 *
 * 	WIDTH ::= NUMBER
 * 	HEIGHT ::= NUMBER
 * 	X ::= NUMBER
 * 	Y ::= NUMBER
 *
 * 	NUMBER ::= [0-9]+
 * 	MAGIC_HASH ::= [0-9a-zA-Z/.-]+
 * </code>
 *
 * Example URLs:
 * - http://i.dawanda.com/fit/7f848c35/Vorsaetze-fuer-2013.jpg  ?crop=true              &size=160x120 &url=http%3A%2F%2Fs32.dawandastatic.com%2FCampaignImage%2F193%2F193734%2F1356687119-440.jpeg
 * - http://i.dawanda.com/fit/8809d1bf/Du-schaffst-es.jpg       ?crop=true              &size=336x252 &url=http%3A%2F%2Fs31.dawandastatic.com%2FCampaignImage%2F190%2F190098%2F1354885195-841.jpeg
 * - http://i.dawanda.com/crop/9b9a3da6/Vorsaetze-fuer-2013.jpg ?crop=589x442%2B9%2B140 &size=336x252 &url=http%3A%2F%2Fs32.dawandastatic.com%2FCampaignImage%2F193%2F193686%2F1356685759-501.jpeg
 */

#include <x0/daemon/XzeroPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/FileSource.h>
#include <x0/Actor.h>
#include <x0/Url.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <wand/MagickWand.h>

using namespace x0;

#if 1 //!defined(XZERO_NDEBUG)
#	define TRACE(level, msg...) { \
		static_assert((level) >= 1 && (level) <= 5, "TRACE()-level must be between 1 and 5, matching Severity::debugN values."); \
		log(Severity::debug ## level, msg); \
	}
#else
#	define TRACE(msg...) /*!*/
#endif

// {{{ UrlFetcher API
class UrlFetcher :
	public HttpMessageProcessor
{
public:
	static UrlFetcher* fetch(HttpWorker* worker, const Url& url, const std::function<void()>& completionHandler);

	UrlFetcher(HttpWorker* worker, const Url& url, const std::function<void()>& completionHandler);

private:
	void io(ev::io& io, int revents);
	void timeout(ev::timer&);

private:
	Buffer writeBuffer_;
	size_t writePos_;

	Buffer readBuffer_;
	size_t readPos_;
};
// }}}
// {{{ Imageable API
class Imageable
{
private:
	HttpRequest* request_;
	MagickWand* wand_;

public:
	explicit Imageable(HttpRequest* r);
	~Imageable();

	void perform();
	void processImage();

private:
	void wandError();
};
// }}}
// {{{ ImageableProcessor Actor API
class ImageableProcessor :
	public Actor<Imageable*>
{
protected:
	virtual void process(Imageable* imageable);
};
// }}}

// {{{ UrlFetcher impl
// }}}
// {{{ Imageable impl
void Imageable::wandError()
{
	ExceptionType severity;
	char* description = MagickGetException(wand_, &severity);

	request_->log(Severity::error, "%s %s %lu %s\n", GetMagickModule(), description);

	MagickRelinquishMemory(description);

	request_->status = HttpStatus::InternalServerError;
	request_->finish();

	delete this;
}

Imageable::Imageable(HttpRequest* r) :
	request_(r),
	wand_(NewMagickWand())
{
}

Imageable::~Imageable()
{
	DestroyMagickWand(wand_);
}

void Imageable::perform()
{
	auto args = Url::parseQuery(request_->query);
#ifndef XZERO_NDEBUG
	request_->log(Severity::debug1, "url: %s", args["url"].c_str());
	request_->log(Severity::debug1, "size: %s", args["size"].c_str());
	request_->log(Severity::debug1, "x: %s", args["x"].c_str());
	request_->log(Severity::debug1, "y: %s", args["y"].c_str());
#endif

	processImage();
}

void Imageable::processImage()
{
	MagickBooleanType status = MagickReadImage(wand_, request_->fileinfo->path().c_str());

	if (status == MagickFalse) {
		wandError();
		return;
	}

	int height = MagickGetImageHeight(wand_);
	int width = MagickGetImageWidth(wand_);
//	int iterations = MagickGetImageIterations(wand_);
	double x, y;
	MagickGetImageResolution(wand_, &x, &y);

	//TRACE(1, "width:%d, height:%d, iters:%d, x:%.2f, y:%.2f", width, height, iterations, x, y);

	MagickResetIterator(wand_);

	while (MagickNextImage(wand_) != MagickFalse) {
		//TRACE(1, "Resizing Image ...");
		printf("image format: %s\n", MagickGetImageFormat(wand_));
		MagickResizeImage(wand_, width * 1.5, height, LanczosFilter, 1.0);
	}

	std::string targetPath("/tmp/image.out");
	status = MagickWriteImages(wand_, targetPath.c_str(), MagickTrue);
	if (status == MagickFalse) {
		wandError();
		return;
	}

	auto fileinfo = request_->connection.worker().fileinfo(targetPath);
	if (!fileinfo) {
		request_->status = HttpStatus::InternalServerError;
		request_->finish();
		delete this;
		return;
	}

	request_->responseHeaders.push_back("Content-Type", fileinfo->mimetype());
	request_->responseHeaders.push_back("Content-Length", x0::lexical_cast<std::string>(fileinfo->size()));

	request_->status = HttpStatus::Ok;

	int fd = fileinfo->open(O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		request_->log(Severity::error, "Could not open file: '%s': %s", fileinfo->filename().c_str(), strerror(errno));
		request_->status = HttpStatus::InternalServerError;
		request_->finish();
		delete this;
		return;
	}

	posix_fadvise(fd, 0, fileinfo->size(), POSIX_FADV_SEQUENTIAL);
	request_->write<FileSource>(fd, 0, fileinfo->size(), true);
	request_->finish();

	delete this;
}
// }}}
// {{{ ImageableProcessor Actor impl
void ImageableProcessor::process(Imageable* imageable)
{
	// perform the actual resize-fit-crop action, then pass response transfer job back to http worker.
	imageable->perform();
}
// }}}
// {{{ ImageablePlugin
class ImageablePlugin :
	public XzeroPlugin
{
private:
	ImageableProcessor* processor_;

public:
	ImageablePlugin(HttpServer& srv, const std::string& name) :
		XzeroPlugin(srv, name),
		processor_(nullptr)
	{
		registerSetupFunction<ImageablePlugin, &ImageablePlugin::setWorkers>("imageable.workers");
		registerSetupFunction<ImageablePlugin, &ImageablePlugin::setTTL>("imageable.ttl");
		registerHandler<ImageablePlugin, &ImageablePlugin::handleRequest>("imageable");

		MagickWandGenesis();
	}

	~ImageablePlugin()
	{
		MagickWandTerminus();
	}

private:
	void setWorkers(const FlowParams& args, FlowValue& result)
	{
	}

	void setTTL(const FlowParams& args, FlowValue& result)
	{
	}

	virtual bool handleRequest(HttpRequest* r, const FlowParams& args)
	{
		processor_->push_back(new Imageable(r));
		return true;
	}
};
X0_EXPORT_PLUGIN_CLASS(ImageablePlugin)
// }}}
