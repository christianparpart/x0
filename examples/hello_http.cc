#include <xzero/Application.h>
#include <xzero/Flags.h>
#include <xzero/http/HttpService.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/ExceptionHandler.h>
#include <xzero/logging.h>

using namespace xzero;

bool helloWorld(http::HttpRequest* request, http::HttpResponse* response) {
  if (request->method() != http::HttpMethod::GET)
    return false;

  const std::string& message = "Hello, World\n";

  response->setStatus(http::HttpStatus::Ok);
  response->setContentLength(message.size());
  response->write(message);
  response->completed();

  return true;
}

int main(int argc, const char* argv[]) {
  Flags flags;
  flags.defineBool("help", 'h', "Prints this help and exits.");
  flags.defineNumber("port", 'p', "PORT", "HTTP port to listen on", 8080);
  flags.defineString("log-level", 'L', "ENUM", "Defines the minimum log level.", "info");

  if (std::error_code ec = flags.parse(argc, argv); ec) {
    logError("Failed to parse flags. $0", ec.message());
    return EXIT_FAILURE;
  }

  if (flags.getBool("help")) {
    std::cout
      << std::endl
      << "Usage: service_demo [options ...]" << std::endl
      << std::endl
      << "Options:" << std::endl
      << flags.helpText()
      << std::endl;
    return EXIT_SUCCESS;
  }

  Logger::get()->setMinimumLogLevel(make_loglevel(flags.getString("log-level")));
  Logger::get()->addTarget(ConsoleLogTarget::get());

  // constructs a single threaded native event loop
  NativeScheduler scheduler{CatchAndLogExceptionHandler{"hello"}};

  // constructs the HTTP service
  http::HttpService service{&scheduler, (int) flags.getNumber("port")};

  // adds a basic handler
  service.addHandler(helloWorld);

  // install a shutdown handler
  service.addHandler([&](auto request, auto response) -> bool {
    if (request->method() == http::HttpMethod::POST && request->path() == "/shutdown") {
      response->setStatus(http::HttpStatus::NoContent);
      response->completed();
      service.stop();
      return true;
    }
    return false;
  });

  // starts the listener
  service.start();

  logInfo("Start serving on port $0 ...", flags.getNumber("port"));

  // run the event loop as long as something should be watched on
  scheduler.runLoop();

  logInfo("Good bye.");
  return EXIT_SUCCESS;
}
