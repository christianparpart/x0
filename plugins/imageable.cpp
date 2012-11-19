/* <x0/plugins/imageresizer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
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
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/FileSource.h>
#include <x0/Actor.h>
#include <x0/Url.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <wand/MagickWand.h>

#define TRACE(msg...) DEBUG("imageable: " msg)

class Imageable
{
private:
	x0::HttpRequest* request_;
	MagickWand* wand_;

public:
	explicit Imageable(x0::HttpRequest* r);
	~Imageable();

	void perform();

private:
	void wandError();
};

void Imageable::wandError()
{
	ExceptionType severity;
	char* description = MagickGetException(wand_, &severity);

	request_->log(x0::Severity::error, "%s %s %lu %s\n", GetMagickModule(), description);

	MagickRelinquishMemory(description);

	request_->status = x0::HttpStatus::InternalServerError;
	request_->finish();

	delete this;
}

Imageable::Imageable(x0::HttpRequest* r) :
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
	MagickBooleanType status = MagickReadImage(wand_, request_->fileinfo->path().c_str());

	if (status == MagickFalse) {
		wandError();
		return;
	}

	int height = MagickGetImageHeight(wand_);
	int width = MagickGetImageWidth(wand_);
	int iterations = MagickGetImageIterations(wand_);
	double x, y;
	MagickGetImageResolution(wand_, &x, &y);

	printf("width:%d, height:%d, iters:%d, x:%.2f, y:%.2f\n",
		width, height, iterations, x, y);

	MagickResetIterator(wand_);

	while (MagickNextImage(wand_) != MagickFalse) {
		TRACE("Resizing Image ...");
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
		request_->status = x0::HttpStatus::InternalServerError;
		request_->finish();
		delete this;
		return;
	}

	request_->responseHeaders.push_back("Content-Type", fileinfo->mimetype());
	request_->responseHeaders.push_back("Content-Length", x0::lexical_cast<std::string>(fileinfo->size()));

	request_->status = x0::HttpStatus::Ok;

	int fd = fileinfo->open(O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		request_->log(x0::Severity::error, "Could not open file: '%s': %s", fileinfo->filename().c_str(), strerror(errno));
		request_->status = x0::HttpStatus::InternalServerError;
		request_->finish();
		delete this;
		return;
	}

	posix_fadvise(fd, 0, fileinfo->size(), POSIX_FADV_SEQUENTIAL);
	request_->write<x0::FileSource>(fd, 0, fileinfo->size(), true);
	request_->finish();

	delete this;
}

class UrlFetcher :
	public x0::HttpMessageProcessor
{
public:
	static UrlFetcher* fetch(x0::HttpWorker* worker, const x0::Url& url, const std::function<void()>& completionHandler);

	UrlFetcher(x0::HttpWorker* worker, const x0::Url& url, const std::function<void()>& completionHandler);

private:
	void io(ev::io& io, int revents);
	void timeout(ev::timer&);
};

class ImageableProcessor :
	public x0::Actor<Imageable*>
{
protected:
	virtual void process(Imageable* imageable);
};

void ImageableProcessor::process(Imageable* imageable)
{
	// perform the actual resize-fit-crop action, then pass response transfer job back to http worker.
	imageable->perform();
}

class ImageablePlugin :
	public x0::HttpPlugin
{
private:
	ImageableProcessor* processor_;

public:
	ImageablePlugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		processor_(nullptr)
	{
		registerHandler<ImageablePlugin, &ImageablePlugin::handleRequest>("imageable");

		MagickWandGenesis();
	}

	~ImageablePlugin()
	{
		MagickWandTerminus();
	}

private:
	virtual bool handleRequest(x0::HttpRequest* r, const x0::FlowParams& args)
	{
		if (!r->fileinfo->isRegular())
			return false;

		if (r->testDirectoryTraversal())
			return true;

//		processor_->push_back(new Imageable(r));
		Imageable* ir = new Imageable(r);
		ir->perform();

		return true;
	}
};

X0_EXPORT_PLUGIN_CLASS(ImageablePlugin)
