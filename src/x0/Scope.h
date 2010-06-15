#ifndef sw_x0_scope_hpp
#define sw_x0_scope_hpp (1)

#include <x0/Types.h>
#include <x0/Api.h>

#include <memory>
#include <map>

namespace x0 {

class X0_API ScopeValue :
	public CustomData
{
public:
	virtual void merge(const ScopeValue *from) = 0;
};

class X0_API Scope :
	public ScopeValue
{
public:
	explicit Scope(const std::string& id);

	template<typename T> T *acquire(const void *key);
	template<typename T> T *get(const void *key) const;
	void set(const void *key, std::shared_ptr<ScopeValue> value);
	void release(const void *key);

	void merge(const ScopeValue *from);

	std::string id() const;

private:
	std::map<const void *, std::shared_ptr<ScopeValue>> data_;
	std::string id_;
};

// {{{ inlines
inline Scope::Scope(const std::string& id) :
	id_(id)
{
}

inline std::string Scope::id() const
{
	return id_;
}

template<typename T> T *Scope::get(const void *key) const
{
	const auto i = data_.find(key);

	if (i != data_.end())
		return static_cast<T *>(i->second.get());

	return 0;
}

template<typename T> T *Scope::acquire(const void *key)
{
	if (T *value = get<T>(key))
		return value;

	set(key, std::make_shared<T>());

	return get<T>(key);
}
// }}}

} // namespace x0

#endif
