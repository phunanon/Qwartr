
#define MAX_CODE_LEN  2000
#define MAX_STACK_LEN 2000

enum Op : uint8_t {
  Op_Return, Op_Mark,   Op_Dupe,   Op_U08,    Op_I32,    Op_F32,
  Op_IAdd,   Op_ISub,   Op_ILthn,  Op_IMthn,  Op_When,   Op_Else,
  Op_Call,   Op_Str,    Op_Print
};
enum Val : uint8_t {
  V_Mark = 0x02, V_U08 = 0x01,  V_I32 = 0x14,  V_F32  = 0x24,
  V_Blob = 0x00
};

typedef uint32_t hash32;
typedef uint16_t hash16;
typedef uint16_t cptr;
typedef uint16_t sptr;
typedef uint16_t flen;
typedef uint16_t vlen;

uint8_t code[MAX_CODE_LEN] = {
  //Entry
  0x00, 0x00, 0x00, 0x00,          //entry
  0x0B, 0x00,                      //Length
  0x00,                            //arity:returns
  Op_I32,  0x17, 0x00, 0x00, 0x00, //[i32 23]
  Op_Call, 0x01, 0x00, 0x00, 0x00, //[call Fib]
  Op_Print,                        //[Print]
  //:1:1 Fib =n n 3 <i ? 1 ! n 1 -i Fib n 2 -i Fib +i ;
  //: Fib DUP 3 <i ? 1 ! DUP 1 -i Fib NIP 2 -i Fib +i ;
  0x01, 0x00, 0x00, 0x00,          //hashed Fib
  0x36, 0x00,                      //Length
  0x11,                            //arity:returns
  Op_Mark, 0x00, 0x01,             //=n
  Op_Dupe, 0x00, 0x01,             //n 
  Op_I32,  0x03, 0x00, 0x00, 0x00, //[i32 3]
  Op_ILthn,                        //[<i]
  Op_When, 0x08, 0x00,             //[? skip 8]
  Op_I32,  0x01, 0x00, 0x00, 0x00, //[i32 1]
  Op_Else, 0x21, 0x00,             //[! skip 33]
  Op_Dupe, 0x00, 0x01,             //n
  Op_I32,  0x01, 0x00, 0x00, 0x00, //[i32 1]
  Op_ISub,                         //[-i]
  Op_Call, 0x01, 0x00, 0x00, 0x00, //[call Fib]
  Op_Dupe, 0x00, 0x01,             //n
  Op_I32,  0x02, 0x00, 0x00, 0x00, //[i32 2]
  Op_ISub,                         //[-i]
  Op_Call, 0x01, 0x00, 0x00, 0x00, //[call Fib]
  Op_IAdd,                         //[+i]
  Op_Return                        //[return]
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
  stack[++s] = V_U08;
}
void pushI32 (cptr &s, int32_t v) {
  memcpy(stack + ++s, &v, sizeof(int32_t));
  stack[s += 4] = V_I32;
}

void exeFunc (hash32 fHash, sptr s) {
  cptr c = findFuncCode(fHash);
  sptr callS = s;
  uint8_t arity = (code[c - 1] & 0xF0) >> 4;
  uint8_t nReturn = code[c - 1] & 0x0F;

  while (true) {
    switch (code[c++]) {
      case Op_Return: //Collapse the last `nReturns` values into the `arity`
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
      case Op_Mark: //Copy mark hash onto the stack
        ++s;
        memcpy(stack + s, code + c, sizeof(hash16));
        stack[s += sizeof(hash16)] = V_Mark;
        c += sizeof(hash16);
        break;
      case Op_Dupe: { //Find marked data on the stack and duplicate it here
        hash16 mark = u16_(code + c);
        for (sptr ss = s; ss;) {
          if (stack[ss] != V_Mark) {
            skipBack(ss);
            continue;
          }
          hash16 hash = u16_(stack + ss - sizeof(hash16));
          if (mark == hash) {
            ss -= sizeof(hash16) + 1; //Go before the mark
            vlen len = vLen(ss);
            ss -= len; //Go before the data
            ++s;
            memcpy(stack + s, stack + ss, len + 1); //Copy data and its datatype
            s += len;
            break;
          }
        }
        c += sizeof(hash16);
        break;
      }
      case Op_U08:
        break;
      case Op_I32: //Copy i32 onto the stack
        ++s;
        memcpy(stack + s, code + c, sizeof(int32_t));
        stack[s += 4] = V_I32;
        c += sizeof(int32_t);
        break;
      case Op_F32:
        break;
      case Op_IAdd:
        pushI32(s, popI32(s) + popI32(s));
        break;
      case Op_ISub: {
        int32_t b = popI32(s);
        pushI32(s, popI32(s) - b);
      } break;
      case Op_ILthn: //pop ints a, b, compare a < b
        pushU08(s, popI32(s) > popI32(s));
        break;
      case Op_IMthn:
        break;
      case Op_When: //pop bool, skip if false
        c += (popU08(s) ? 0 : u16_(code + c)) + sizeof(flen);
        break;
      case Op_Else: //Unconditionally skip
        c += u16_(code + c) + sizeof(flen);
        break;
      case Op_Call:
        exeFunc(i32_(code + c), s);
        c += sizeof(hash32);
        break;
      case Op_Str:
        break;
      case Op_Print:
    Serial.println(popI32(s));
        break;
    }
  }
}
