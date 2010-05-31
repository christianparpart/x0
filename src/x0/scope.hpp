#ifndef sw_x0_scope_hpp
#define sw_x0_scope_hpp (1)

#include <x0/types.hpp>
#include <x0/api.hpp>

#include <memory>
#include <map>

namespace x0 {

class X0_API scope_value :
	public custom_data
{
public:
	virtual void merge(const scope_value *from) = 0;
};

class X0_API scope :
	public scope_value
{
public:
	explicit scope(const std::string& id);

	template<typename T> T *acquire(void *key);
	template<typename T> T *get(void *key) const;
	void set(void *key, std::shared_ptr<scope_value> value);
	void release(void *key);

	void merge(const scope_value *from);

	std::string id() const;

private:
	std::map<void *, std::shared_ptr<scope_value>> data_;
	std::string id_;
};

// {{{ inlines
inline scope::scope(const std::string& id) :
	id_(id)
{
}

inline std::string scope::id() const
{
	return id_;
}

template<typename T> T *scope::get(void *key) const
{
	auto i = data_.find(key);

	if (i != data_.end())
		return static_cast<T *>(i->second.get());

	return 0;
}

template<typename T> T *scope::acquire(void *key)
{
	if (T *value = get<T>(key))
		return value;

	set(key, std::make_shared<T>());

	return get<T>(key);
}

inline void scope::release(void *key)
{
	data_.erase(data_.find(key));
}
// }}}

} // namespace x0

#endif
