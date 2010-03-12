#ifndef sw_x0_fileinfo_hpp
#define sw_x0_fileinfo_hpp (1)

#include <x0/api.hpp>

#include <string>
#include <map>

#include <ev++.h>

namespace x0 {

class fileinfo_service;
class plugin;

//! \addtogroup core
//@{

/** file info cache object.
 *
 * \see fileinfo_service, server
 */
class X0_API fileinfo
{
private:
	fileinfo(const fileinfo&) = delete;
	fileinfo& operator=(const fileinfo&) = delete;

private:
	fileinfo_service& service_;
	ev::stat watcher_;

	std::string filename_;

	bool exists_;
	mutable std::string etag_;
	mutable std::string mtime_;
	mutable std::string mimetype_;

	std::map<const plugin *, void *> data_;

	friend class fileinfo_service;

public:
	fileinfo(fileinfo_service& service, const std::string& filename);

	std::string filename() const;

	std::size_t size() const;
	time_t mtime() const;

	bool exists() const;
	bool is_directory() const;
	bool is_regular() const;
	bool is_executable() const;

	const ev_statdata * operator->() const;

	// custom-data
	void bind(const plugin *self, void *data);
	template<typename T> T& operator()(const plugin *self) const;
	template<typename T> T& get(const plugin *self) const;
	void unbind(const plugin *self);

	// HTTP related high-level properties
	std::string etag() const;
	std::string last_modified() const;
	std::string mimetype() const;

private:
	std::string get_mime_type(std::string ext) const;

	void callback(ev::stat& w, int revents);
};

//@}

} // namespace x0

#include <x0/io/fileinfo.ipp>

#endif
