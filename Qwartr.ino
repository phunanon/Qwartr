
#pragma GCC optimize ("O3")
#pragma GCC push_options

#define MAX_CODE_LEN  2000
#define MAX_STACK_LEN 2000

enum Op : uint8_t {
  Op_Return, Op_Mark,   Op_Var,    Op_U08,    Op_I32,    Op_Blob,
  Op_Add,    Op_Sub,    Op_Lthn,   Op_Mthn,   Op_IAdd,   Op_ISub,   Op_ILthn,
  Op_IMthn,  Op_When,   Op_Else,   Op_Call,   Op_Str,    Op_Print,  Op_DigW,
  Op_DigR,   Op_Sleep,
};
enum Val : uint8_t {
  V_Mark = 0x02, V_U08 = 0x01,  V_I32 = 0x14,  V_F32  = 0x24,
  V_Blob = 0x00,
};

typedef uint32_t hash32;
typedef uint16_t hash16;
typedef uint16_t cptr;
typedef uint16_t sptr;
typedef uint16_t flen;
typedef uint16_t vlen;

uint8_t code[MAX_CODE_LEN] = {
  //:1:1 Fib =n n 3 <i ? 1 ! n 1 -i Fib n 2 -i Fib +i ;
  0x96, 0xEA, 0x31, 0x00, 0x11,   //Func Fib
  0x01, 0x13, 0xB6,               //Mark =n
  0x02, 0x13, 0xB6,               //Var n
  0x04, 0x03, 0x00, 0x00, 0x00,   //Push I32 3
  0x0C,                           //<i
  0x0E, 0x08, 0x00,               //?
  0x04, 0x01, 0x00, 0x00, 0x00,   //Push I32 1
  0x0F, 0x19, 0x00,               //!
  0x02, 0x13, 0xB6,               //Var n
  0x04, 0x01, 0x00, 0x00, 0x00,   //Push I32 1
  0x0B,                           //-i
  0x10, 0x96, 0xEA,               //Call Fib
  0x02, 0x13, 0xB6,               //Var n
  0x04, 0x02, 0x00, 0x00, 0x00,   //Push I32 2
  0x0B,                           //-i
  0x10, 0x96, 0xEA,               //Call Fib
  0x0A,                           //+i
  0x00, 
  0x04, 0x17, 0x00, 0x00, 0x00,   //Push I32 23
  0x10, 0x96, 0xEA,               //Call Fib
  0x12,                           //PRT
  0x09, 0x00,                     //Entry length
};
cptr codeLen = 65;
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
  while (!Serial);
  Serial.begin(115200);
  Serial.println("Hello.");
  auto timeStart = millis();
  exeEntry();
  auto timeFinish = millis();
  Serial.print(timeFinish - timeStart);
  Serial.println("ms");
}
void loop() {}

hash16 prevHash = 0;
cptr prevCptr = sizeof(hash16) + sizeof(flen) + 1;
cptr findFunc (hash16 hash) {
  if (hash == prevHash)
    return prevCptr;
  cptr c = 0;
  while (c < codeLen && u16_(code + c) != hash) {
    c += sizeof(hash16);
    c += sizeof(flen) + 1 + u16_(code + c);
  }
  c += sizeof(hash16) + sizeof(flen) + 1;
  prevHash = hash;
  prevCptr = c;
  return c;
}

vlen vLen (sptr s) {
  vlen len = stack[s] & 0x0F;
  return len ? len : u16_(stack + s - sizeof(vlen)) + sizeof(vlen);
}
#define skipBack(s) \
  s -= vLen(s - 1) + 1
int32_t popI32 (cptr &s) {
  s -= sizeof(int32_t) + 1;
  int32_t i32 = i32_(stack + s);
  return i32;
}
#define popU08(s) \
  stack[s -= sizeof(uint8_t) + 1]
void pushU08 (cptr &s, uint8_t v) {
  stack[s++] = v;
  stack[s++] = V_U08;
}
void pushI32 (cptr &s, int32_t v) {
  memcpy(stack + s, &v, sizeof(int32_t));
  stack[s += sizeof(int32_t)] = V_I32;
  ++s;
}

void exeEntry () {
  cptr entryLen = u16_(code + codeLen - sizeof(uint16_t));
  exeFunc(codeLen - entryLen - sizeof(uint16_t), 0, 0, 0);
  codeLen -= entryLen + sizeof(uint16_t);
}

void exeFunc (cptr c, sptr s, vlen arity, vlen nReturn) {
  sptr callS = s;

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
          memcpy(stack + copyTo, stack + copyFrom, s - copyFrom);
        }
        return;
      case Op_Mark: //Copy mark hash onto the stack
        memcpy(stack + s, code + c, sizeof(hash16));
        stack[s += sizeof(hash16)] = V_Mark;
        ++s;
        c += sizeof(hash16);
        break;
      case Op_Var: { //Find marked data on the stack and duplicate it here
        hash16 mark = u16_(code + c);
        sptr ss = s;
        while (ss) {
          if (stack[ss - 1] != V_Mark) {
            skipBack(ss);
            continue;
          }
          if (mark == u16_(stack + ss - sizeof(hash16) - 1)) {
            ss -= sizeof(hash16) + 1;
            vlen len = vLen(ss - 1) + 1;
            memcpy(stack + s, stack + ss - len, len); //Copy data and its datatype
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
        memcpy(stack + s, code + c, sizeof(int32_t));
        stack[s += sizeof(int32_t)] = V_I32;
        ++s;
        c += sizeof(int32_t);
        break;
      case Op_Add:
        pushU08(s, popU08(s) + popU08(s));
        break;
      case Op_IAdd:
        pushI32(s, popI32(s) + popI32(s));
        break;
      case Op_ISub: {
        int32_t b = popI32(s);
        pushI32(s, popI32(s) - b);
      } break;
      case Op_ILthn: {
        int32_t b = popI32(s);
        pushU08(s, popI32(s) < b);
      } break;
      case Op_IMthn: {
        int32_t b = popI32(s);
        pushU08(s, popI32(s) > b);
      } break;
      case Op_When: //pop bool, skip if false
        c += (popU08(s) ? 0 : u16_(code + c)) + sizeof(flen);
        break;
      case Op_Else: //Unconditionally skip
        c += u16_(code + c) + sizeof(flen);
        break;
      case Op_Call: {
          cptr fc = findFunc(u16_(code + c));
          c += sizeof(hash16);
          exeFunc(fc, s, (code[fc - 1] & 0xF0) >> 4, code[fc - 1] & 0x0F);
        } break;
      case Op_Str:
        break;
      case Op_Print:
        Serial.println(popI32(s)); //TODO print string not i32
        break;
      case Op_DigW:
        digitalWrite(popU08(s), popU08(s));
        break;
      case Op_DigR:
        pushU08(s, digitalRead(popU08(s)));
        break;
      case Op_Sleep:
        delay(popI32(s));
        break;
    }
  }
}
