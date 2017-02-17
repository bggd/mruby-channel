struct Variant {

  enum class Type {
    UNKNOWN,
    NIL,
    BOOL,
    FIXNUM,
    FLOAT,
    STRING,
    SYMBOL,
    ARRAY,
    HASH,
    CLOSE
  };

  Variant::Type type;

  union Data {
    bool boolean;
    mrb_int integer;
    mrb_float number;
    struct {
      char* str;
      mrb_int len;
    };
    std::vector<Variant>* ary;
    std::vector<std::pair<Variant, Variant>>* tbl;
  } data;

  Variant();
  ~Variant();
  Variant(const Variant& other);
  Variant& operator=(const Variant& other);

  Variant(bool boolean);
  Variant(mrb_int integer);
  Variant(mrb_float number);
  Variant(Variant::Type type, char* str, mrb_int len);
  Variant(std::vector<Variant>* ary);
  Variant(std::vector<std::pair<Variant, Variant>>* tbl);

  static Variant from_mruby(mrb_state* mrb, mrb_value v);
  mrb_value to_mruby(mrb_state* mrb);

  void free();

};
