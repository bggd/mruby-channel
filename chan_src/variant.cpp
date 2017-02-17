Variant::Variant()
{
  this->type = Variant::Type::UNKNOWN;
}

Variant::~Variant()
{
}

Variant::Variant(const Variant& other)
{
  *this = other;
}

Variant& Variant::operator=(const Variant& other)
{
  this->type = other.type;
  this->data = other.data;
  return *this;
}

Variant::Variant(bool boolean)
{
  this->type = Variant::Type::BOOL;
  this->data.boolean = boolean;
}

Variant::Variant(mrb_int integer)
{
  this->type = Variant::Type::FIXNUM;
  this->data.integer = integer;
}

Variant::Variant(mrb_float number)
{
  this->type = Variant::Type::FLOAT;
  this->data.number = number;
}

Variant::Variant(Variant::Type type, char* str, mrb_int len)
{
  this->type = type;
  this->data.str = new char[len+1];
  this->data.len = len;
  memcpy(this->data.str, str, len);
}

Variant::Variant(std::vector<Variant>* ary)
{
  this->type = Variant::Type::ARRAY;
  this->data.ary = ary;
}

Variant::Variant(std::vector<std::pair<Variant, Variant>>* tbl)
{
  this->type = Variant::Type::HASH;
  this->data.tbl = tbl;
}

Variant Variant::from_mruby(mrb_state* mrb, mrb_value v)
{
  std::vector<Variant>* ary = nullptr;
  std::vector<std::pair<Variant, Variant>>* tbl = nullptr;
  const char* str = nullptr;

  if (mrb_nil_p(v)) {
    Variant ret;
    ret.type = Variant::Type::NIL;
    return ret;
  }

  switch (mrb_type(v)) {
    case MRB_TT_TRUE: return Variant(true);
    case MRB_TT_FALSE: return Variant(false);
    case MRB_TT_FIXNUM: return Variant(mrb_fixnum(v));
    case MRB_TT_FLOAT: return Variant(mrb_float(v));
    case MRB_TT_STRING: return Variant(Variant::Type::STRING, RSTRING_PTR(v), RSTRING_LEN(v));
    case MRB_TT_SYMBOL:
      mrb_int len;
      str = mrb_sym2name_len(mrb, mrb_symbol(v), &len);
      return Variant(Variant::Type::SYMBOL, (char*)str, len);
    case MRB_TT_ARRAY:
      ary = new std::vector<Variant>;
      ary->reserve(RARRAY_LEN(v));
      for (mrb_int i=0; i < RARRAY_LEN(v); ++i) {
        ary->push_back(Variant::from_mruby(mrb, RARRAY_PTR(v)[i]));
      }
      return Variant(ary);
    case MRB_TT_HASH:
      mrb_value keys = mrb_hash_keys(mrb, v);
      mrb_value values = mrb_hash_values(mrb, v);
      tbl = new std::vector<std::pair<Variant, Variant>>;
      tbl->reserve(RARRAY_LEN(keys));
      for (mrb_int i=0; i < RARRAY_LEN(keys); ++i) {
        mrb_value k = RARRAY_PTR(keys)[i];
        mrb_value val = RARRAY_PTR(values)[i];
        tbl->push_back(std::make_pair(Variant::from_mruby(mrb, k), Variant::from_mruby(mrb, val)));
      }
      return Variant(tbl);
    default: break;
  }

  return Variant();
}

mrb_value Variant::to_mruby(mrb_state* mrb)
{
  switch (this->type) {
    case Variant::Type::NIL: return mrb_nil_value();
    case Variant::Type::BOOL: return mrb_bool_value(this->data.boolean);
    case Variant::Type::FIXNUM: return mrb_fixnum_value(this->data.integer);
    case Variant::Type::FLOAT: return mrb_float_value(mrb, this->data.number);
    case Variant::Type::STRING:
      mrb_value str = mrb_str_buf_new(mrb, this->data.len);
      return mrb_str_cat(mrb, str, this->data.str, this->data.len);
    case Variant::Type::SYMBOL:
      return mrb_symbol_value(mrb_intern(mrb, this->data.str, this->data.len));
    case Variant::Type::ARRAY:
      mrb_value ary = mrb_ary_new_capa(mrb, this->data.ary->size());
      for (auto it=this->data.ary->begin(); it != this->data.ary->end(); ++it) {
        mrb_ary_push(mrb, ary, it->to_mruby(mrb));
      }
      return ary;
    case Variant::Type::HASH:
      mrb_value tbl = mrb_hash_new_capa(mrb, this->data.tbl->size());
      for (auto it=this->data.tbl->begin(); it != this->data.tbl->end(); ++it) {
        mrb_hash_set(mrb, tbl, it->first.to_mruby(mrb), it->second.to_mruby(mrb));
      }
      return tbl;
    default: break;
  }
  return mrb_nil_value();
}

void Variant::free()
{
  switch (this->type) {
    case Variant::Type::STRING:
    case Variant::Type::SYMBOL:
      delete[] this->data.str;
      break;
    case Variant::Type::ARRAY:
      delete this->data.ary;
      break;
    case Variant::Type::HASH:
      delete this->data.tbl;
      break;
    default: break;
  }
}
