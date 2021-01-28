
#define MAX_CODE_LEN  2000
#define MAX_STACK_LEN 2000

enum Op : uint8_t {
  Return, Mark,   Dupe,   U08,    I32,    F32,
  IAdd,   ISub,   ILthn,  IMthn,  When,   Else,
  Call,   Str,    Print
};
enum Val : uint8_t {
  Mark = 0x04, U08  = 0x01, I32  = 0x14, F32  = 0x24,
  Blob = 0x00
};

typedef uint32_t hash32;
typedef uint16_t cptr;
typedef uint16_t sptr;
typedef uint16_t flen;
typedef uint16_t vlen;

uint8_t code[MAX_CODE_LEN] = {
  //Entry
  0x00, 0x00, 0x00, 0x00,           //entry
  0x0B, 0x00,                       //Length
  0x00,                             //arity:returns
  Op::I32,  0x17, 0x00, 0x00, 0x00, //[i32 23]
  Op::Call, 0x01, 0x00, 0x00, 0x00, //[call Fib]
  Op::Print,                        //[Print]
  //:1:1 Fib =n n 3 <i ? 1 ! n 1 -i Fib n 2 -i Fib +i ;
  0x01, 0x00, 0x00, 0x00,           //hashed Fib
  0x3E, 0x00,                       //Length
  0x11,                             //arity:returns
  Op::Mark, 0x00, 0x00, 0x00, 0x01, //=n
  Op::Dupe, 0x00, 0x00, 0x00, 0x01, //n 
  Op::I32,  0x03, 0x00, 0x00, 0x00, //[i32 3]
  Op::ILthn,                        //[<i]
  Op::When, 0x08, 0x00,             //[? skip 8]
  Op::I32,  0x01, 0x00, 0x00, 0x00, //[i32 1]
  Op::Else, 0x21, 0x00,             //[! skip 33]
  Op::Dupe, 0x00, 0x00, 0x00, 0x01, //n
  Op::I32,  0x01, 0x00, 0x00, 0x00, //[i32 1]
  Op::ISub,                         //[-i]
  Op::Call, 0x01, 0x00, 0x00, 0x00, //[call Fib]
  Op::Dupe, 0x00, 0x00, 0x00, 0x01, //n
  Op::I32,  0x02, 0x00, 0x00, 0x00, //[i32 2]
  Op::ISub,                         //[-i]
  Op::Call, 0x01, 0x00, 0x00, 0x00, //[call Fib]
  Op::IAdd,                         //[+i]
  Op::Return                        //[return]
};
uint8_t stack[MAX_STACK_LEN];

uint16_t u16_ (uint8_t* b) {
  uint16_t u16;
  memcpy(&u16, b, sizeof(uint16_t));
  return u16;
}
int32_t i32_ (uint8_t* b) {
  int32_t i32;
  memcpy(&i32, b, sizeof(int32_t));
  return i32;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Hello.");
  auto timeStart = millis();
  exeFunc(0, 0); //Call entry
  auto timeFinish = millis();
  Serial.print(timeFinish - timeStart);
  Serial.println("ms");
}
void loop() {}

hash32 prevHash = 0;
cptr prevCptr = sizeof(hash32) + sizeof(flen) + 1;
cptr findFuncCode (hash32 hash) {
  if (hash == prevHash)
    return prevCptr;
  cptr c = 0;
  while (c < MAX_CODE_LEN && i32_(code + c) != hash) {
    c += sizeof(hash32);
    c += sizeof(flen) + 1 + u16_(code + c);
  }
  c += sizeof(hash32) + sizeof(flen) + 1;
  prevHash = hash;
  prevCptr = c;
  return c;
}

vlen vLen (sptr s) {
  uint16_t len = stack[s] & 0x0F;
  return len ? len : u16_(stack + s - sizeof(vlen));
}
void skipBack (sptr &s) {
  if (vlen len = vLen(s))
    s -= len + 1;
  else
    s -= u16_(stack + s - sizeof(vlen)) + sizeof(vlen);
}
int32_t popI32 (cptr &s) {
  int32_t i32 = i32_((stack + s) - sizeof(int32_t));
  s -= sizeof(int32_t) + 1;
  return i32;
}
uint8_t popU08 (cptr &s) {
  uint8_t u08 = *((stack + s) - sizeof(uint8_t));
  s -= sizeof(uint8_t) + 1;
  return u08;
}
void pushU08 (cptr &s, uint8_t v) {
  stack[++s] = v;
  stack[++s] = Val::U08;
}
void pushI32 (cptr &s, int32_t v) {
  memcpy(stack + ++s, &v, sizeof(int32_t));
  stack[s += 4] = Val::I32;
}

void exeFunc (hash32 fHash, sptr s) {
  cptr c = findFuncCode(fHash);
  sptr callS = s;
  uint8_t arity = (code[c - 1] & 0xF0) >> 4;
  uint8_t nReturn = code[c - 1] & 0x0F;

  while (true) {
    switch (code[c++]) {
      case Op::Return: //Collapse the last `nReturns` values into the `arity`
        if (nReturn) {
          sptr copyTo = callS;
          for (uint8_t a = 0; a < arity; ++a)
            skipBack(copyTo);
          sptr copyFrom = s;
          for (uint8_t r = 0; r < nReturn; ++r)
            skipBack(copyFrom);
          memcpy(stack + copyTo + 1, stack + copyFrom + 1, s - copyFrom);
        }
        return;
      case Op::Mark: //Copy mark hash onto the stack
        ++s;
        memcpy(stack + s, code + c, sizeof(hash32));
        stack[s += 4] = Val::Mark;
        c += sizeof(hash32);
        break;
      case Op::Dupe: { //Find marked data on the stack and duplicate it here
        hash32 mark = i32_(code + c);
        for (sptr ss = s; ss;) {
          if (stack[ss] != Val::Mark) {
            skipBack(ss);
            continue;
          }
          hash32 hash = i32_(stack + ss - sizeof(hash32));
          if (mark == hash) {
            ss -= sizeof(hash32) + 1; //Go before the mark
            vlen len = vLen(ss);
            ss -= len; //Go before the data
            ++s;
            memcpy(stack + s, stack + ss, len + 1); //Copy data and its datatype
            s += len;
            break;
          }
        }
        c += sizeof(hash32);
        break;
      }
      case Op::U08:
        break;
      case Op::I32: //Copy i32 onto the stack
        ++s;
        memcpy(stack + s, code + c, sizeof(int32_t));
        stack[s += 4] = Val::I32;
        c += sizeof(int32_t);
        break;
      case Op::F32:
        break;
      case Op::IAdd:
        pushI32(s, popI32(s) + popI32(s));
        break;
      case Op::ISub: {
        int32_t b = popI32(s);
        pushI32(s, popI32(s) - b);
      } break;
      case Op::ILthn: //pop ints a, b, compare a < b
        pushU08(s, popI32(s) > popI32(s));
        break;
      case Op::IMthn:
        break;
      case Op::When: //pop bool, skip if false
        c += (popU08(s) ? 0 : u16_(code + c)) + sizeof(flen);
        break;
      case Op::Else: //Unconditionally skip
        c += u16_(code + c) + sizeof(flen);
        break;
      case Op::Call:
        exeFunc(i32_(code + c), s);
        c += sizeof(hash32);
        break;
      case Op::Str:
        break;
      case Op::Print:
    Serial.println(popI32(s));
        break;
    }
  }
}