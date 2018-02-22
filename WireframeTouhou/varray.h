#ifndef VARRAY_H
#define VARRAY_H

// a simplified mix of std::array + std::vector with fixed capacity

template <class _Ty, size_t _Size>
class varray {
  using size_type = size_t;
  using reference = _Ty &;
  using const_reference = const _Ty &;

  size_t _Num;
  _Ty _Elems[_Size];

 public:
  varray() : _Num(0) {}

  reference operator[](size_type _Pos) { return _Elems[_Pos]; }

  const_reference operator[](size_type _Pos) const { return _Elems[_Pos]; }

  void push_back(const _Ty &_Val) {
    if (_Num < _Size) {
      _Elems[_Num++] = _Val;
    }
  }

  void clear() { _Num = 0; }

  size_type size() const { return _Num; }

  size_type capacity() const { return _Size; }

  _Ty *data() { return &_Elems[0]; }

  _Ty *alloc(size_type _N) {
    if (_N + _Num < _Size) {
      _Num += _N;
      return &_Elems[_Num - _N];
    } else {
      return nullptr;
    }
  }

  bool empty() const { return _Num == 0; }

  bool full() const { return _Num == _Size; }
};

#endif
