namespace stout {

template <typename T>
class borrowed_ptr
{
public:
  template <typename U, typename F>
  borrowed_ptr(U* u, F&& f)
    : t_(u, std::forward<F>(f))
  {}

  // NOTE: requires 'Deleter' to be copyable (for now).
  template <typename U, typename Deleter>
  borrowed_ptr(std::unique_ptr<U, Deleter>&& u)
    : borrowed_ptr(
        u.release(),
        [deleter = u.get_deleter()](auto* t) { deleter(t); })
  {}

  template <typename H>
  friend H AbslHashValue(H h, const borrowed_ptr& that) {
    return H::combine(std::move(h), that.t_);
  }

  void relinquish()
  {
    t_.reset();
  }

  T* get() const
  {
    return t_.get();
  }

  T* operator->() const
  {
    return t_.operator->();
  }

  T& operator*() const
  {
    return t_.operator*();
  }

  // TODO(benh): operator[]

private:
  std::unique_ptr<T, std::function<void(T*)>> t_;
};


template <typename T, typename F>
borrowed_ptr<T> borrow(T* t, F&& f)
{
  return borrowed_ptr<T>(t, std::forward<F>(f));
}


template <typename T, typename Deleter>
borrowed_ptr<T> borrow(std::unique_ptr<T, Deleter>&& t)
{
  return borrowed_ptr<T>(std::move(t));
}


template <typename T, typename F>
std::vector<borrowed_ptr<T>> borrow(size_t n, T* t, F&& f)
{
  auto relinquish = [n = new std::atomic<size_t>(n), f = std::forward<F>(f)](T* t) {
    if (n->fetch_sub(1) == 1) {
      f(t);
      delete n;
    }
  };

  std::vector<borrowed_ptr<T>> result;

  result.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    result.push_back(borrow(t, relinquish));
  }

  return result;
}

} // namespace stout {
