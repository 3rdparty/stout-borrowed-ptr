# `stout::borrowed_ptr`

`stout::borrowed_ptr` fills a gap between `std::unique_ptr` and
`std::shared_ptr`.

***It is intended to help programmers write correct and
self-documenting code wrt ownership semantics where you might have
otherwise used pointers and relied on documentation.***

It is especially useful for *asynchronous code*.

Properties:

* Like a `std::unique_ptr`, a `stout::borrowed_ptr` is not copyable
but must be moved.

* Like a `std::shared_ptr`, there can
be more than one instance of `stout::borrowed_ptr` that point to the
same thing.

* Unlike `std::shared_ptr`, because `stout::borrowed_ptr` is not
copyable the reference count for the pointer is only decremented when
the `stout::borrowed_ptr` relinquishes borrowing or is destructed
without having already called `relinquish()`.


## Usage

You "borrow" a pointer using `stout::borrow`:

```cpp
// Assuming you have some pointer 'data'.
auto borrowed = stout::borrow(data, [](auto*) {
 // Invoked when 'borrowed' has been relinquished or been destructed.
});
```

This is great for callbacks that are asynchronous, i.e., when the call
returns you can't be sure that the callee doesn't still need to use
the pointer you passed.

```cpp
callback(std::move(borrowed));

// 'borrowed' might still be getting used!
```

The callee can either let their `stout::borrowed_ptr` go out of scope
or they can explicitly relinquish borrowing by calling `relinquish()`:

```cpp
void callback(stout::borrowed_ptr<Data>&& data) {
  std::async([data = std::move(data)]() {
    // Do some processing with 'data'.
    data.relinquish();
    // Do more processing without 'data'.  
  });
}
```

You can create multiple instances of `stout::borrowed_ptr` when you
want to share it between multiple callees:

```cpp
auto borrows = stout::borrow(4, data, [](auto*) {
 // Invoked when ALL borrowers have relinquished.
});

for (auto&& borrowed : std::move(borrows)) {
  std::async([borrowed = std::move(borrowed)]() {
    // ... borrowed.relinquish();
  });
}
```

You can use `std::unique_ptr` with borrowing semantics as well. If you
don't care about using the `std::unique_ptr` after borrowing has been
relinquished you can simply do:

```cpp
std::unique_ptr<Data> data = ...;

auto borrowed = stout::borrow(std::move(data));
```

However, if you want to "re-own" after borrowing has been relinquished
then you can do:

```cpp
std::unique_ptr<Data> data = ...;

auto borrowed = stout::borrow(std::move(data), [](auto&& data) {
  // Can now use 'data' knowing there are no borrowers.
});
```