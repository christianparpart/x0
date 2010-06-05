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

	template<typename T> T *acquire(void *key);
	template<typename T> T *get(void *key) const;
	void set(void *key, std::shared_ptr<ScopeValue> value);
	void release(void *key);

	void merge(const ScopeValue *from);

	std::string id() const;

private:
	std::map<void *, std::shared_ptr<ScopeValue>> data_;
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

template<typename T> T *Scope::get(void *key) const
{
	auto i = data_.find(key);

	if (i != data_.end())
		return static_cast<T *>(i->second.get());

	return 0;
}

template<typename T> T *Scope::acquire(void *key)
{
	if (T *value = get<T>(key))
		return value;

	set(key, std::make_shared<T>());

	return get<T>(key);
}

inline void Scope::release(void *key)
{
	data_.erase(data_.find(key));
}
// }}}

} // namespace x0

#endif
