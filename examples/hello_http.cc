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

  Application::installGlobalExceptionHandler();
  NativeScheduler scheduler{CatchAndLogExceptionHandler{"main"}};

  http::HttpService service;

  service.addHandler(helloWorld);

  service.configureTcp(&scheduler,
                       &scheduler,
                       20_seconds, // read timeout
                       10_seconds, // write timeout
                       8_seconds, //Duration::Zero, // TCP FIN timeout (FIXME: broken on WSL)
                       IPAddress{"0.0.0.0"},
                       flags.getNumber("port"));

  service.start();

  scheduler.runLoop();
  return EXIT_SUCCESS;
}
