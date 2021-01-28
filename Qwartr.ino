
#define MAX_CODE_LEN  2000
#define MAX_STACK_LEN 2000

#define Op_Return 0x00
#define Op_Mark   0x01
#define Op_Dupe   0x02
#define Op_U08    0x03
#define Op_I32    0x04
#define Op_F32    0x05
#define Op_IAdd   0x06
#define Op_ISub   0x07
#define Op_ILthn  0x08
#define Op_IMthn  0x09
#define Op_When   0x0A
#define Op_Else   0x0B
#define Op_Call   0x0C
#define Op_Str    0x0D
#define Op_Print  0x0E

#define Val_Mark 0x04
#define Val_U08  0x01
#define Val_I32  0x14
#define Val_F32  0x24
#define Val_Blob 0x00

typedef uint32_t hash32;
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
  0x01, 0x00, 0x00, 0x00,          //hashed Fib
  0x3E, 0x00,                      //Length
  0x11,                            //arity:returns
  Op_Mark, 0x00, 0x00, 0x00, 0x01, //=n
  Op_Dupe, 0x00, 0x00, 0x00, 0x01, //n 
  Op_I32,  0x03, 0x00, 0x00, 0x00, //[i32 3]
  Op_ILthn,                        //[<i]
  Op_When, 0x08, 0x00,             //[? skip 8]
  Op_I32,  0x01, 0x00, 0x00, 0x00, //[i32 1]
  Op_Else, 0x21, 0x00,             //[! skip 33]
  Op_Dupe, 0x00, 0x00, 0x00, 0x01, //n
  Op_I32,  0x01, 0x00, 0x00, 0x00, //[i32 1]
  Op_ISub,                         //[-i]
  Op_Call, 0x01, 0x00, 0x00, 0x00, //[call Fib]
  Op_Dupe, 0x00, 0x00, 0x00, 0x01, //n
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
  stack[++s] = Val_U08;
}
void pushI32 (cptr &s, int32_t v) {
  memcpy(stack + ++s, &v, sizeof(int32_t));
  stack[s += 4] = Val_I32;
}

void exeFunc (hash32 fHash, sptr s) {
  cptr c = findFuncCode(fHash);
  sptr callS = s;
  uint8_t arity = (code[c - 1] & 0xF0) >> 4;
  uint8_t nReturn = code[c - 1] & 0x0F;
  
  const void* ops[] PROGMEM = {
    &&op_Return, &&op_Mark,  &&op_Dupe, &&op_U08,
    &&op_I32,    &&op_F32,   &&op_IAdd, &&op_ISub,
    &&op_ILthn,  &&op_IMthn, &&op_When, &&op_Else,
    &&op_Call,   &&op_Str,   &&op_Print
  };
  #define NEXT_OP() goto *ops[code[c++]]

  NEXT_OP();
  op_Return: //Collapse the last `nReturns` values into the `arity`
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
  op_Mark: //Copy mark hash onto the stack
    ++s;
    memcpy(stack + s, code + c, sizeof(hash32));
    stack[s += 4] = Val_Mark;
    c += sizeof(hash32);
    NEXT_OP();
  op_Dupe: { //Find marked data on the stack and duplicate it here
    hash32 mark = i32_(code + c);
    for (sptr ss = s; ss;) {
      if (stack[ss] != Val_Mark) {
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
    NEXT_OP();
  }
  op_U08:
    NEXT_OP();
  op_I32: //Copy i32 onto the stack
    ++s;
    memcpy(stack + s, code + c, sizeof(int32_t));
    stack[s += 4] = Val_I32;
    c += sizeof(int32_t);
    NEXT_OP();
  op_F32:
    NEXT_OP();
  op_IAdd:
    pushI32(s, popI32(s) + popI32(s));
  NEXT_OP();
  op_ISub: {
    int32_t b = popI32(s);
    pushI32(s, popI32(s) - b);
  } NEXT_OP();
  op_ILthn: //pop ints a, b, compare a < b
    pushU08(s, popI32(s) > popI32(s));
    NEXT_OP();
  op_IMthn:
    NEXT_OP();
  op_When: //pop bool, skip if false
    c += (popU08(s) ? 0 : u16_(code + c)) + sizeof(flen);
    NEXT_OP();
  op_Else: //Unconditionally skip
    c += u16_(code + c) + sizeof(flen);
    NEXT_OP();
  op_Call:
    exeFunc(i32_(code + c), s);
    c += sizeof(hash32);
    NEXT_OP();
  op_Str:
    NEXT_OP();
  op_Print:
Serial.println(popI32(s));
    NEXT_OP();
}
