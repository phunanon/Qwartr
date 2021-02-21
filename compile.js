#!/usr/bin/node
const ops = (
  "RETURN MARK VAR U08 I32 BLOB + - < > +i -i <i >i ? ! " +
  "CALL STR PRT DIGW DIGR SLEEP")
  .split(" ")
  .reduce((acc, o, i) => ({ [o]: i, ...acc }), {});
const hash32 = str =>
  [...str].reduce((h, c) => (h << 5) + h + c.charCodeAt(0), 5381);
const num16bin = num => [num & 0xff, (num & 0xff00) >> 8];
const hash16bin = str => num16bin(hash32(str));
const toHex = n => (n < 0x10 ? "0" : "") + n.toString(16).toUpperCase();
const invisiComp = /^[\n\t.]/;
function i32bin(a) {
  const arr = new ArrayBuffer(4);
  new DataView(arr).setInt32(0, a, 1);
  return [...new Uint8Array(arr)];
}
function u16bin(a) {
  const arr = new ArrayBuffer(2);
  new DataView(arr).setUint16(0, a, 1);
  return [...new Uint8Array(arr)];
}

const code = require("fs").readFileSync(process.argv[2], "utf8");
const [stringless, strings] = code.split(/(?<!\\)"/g).reduce(
  ([code, strings], block, i) => {
    if (i % 2) {
      strings.push(block);
      code.push('"');
    } else code.push(block);
    return [code, strings];
  },
  [[], []]
);
const clean = stringless.join(" ").match(/[^\n\s]+/g);
let compiled = [];
for (let w = 0; w < clean.length; ++w) {
  if (clean[w].startsWith(":")) {
    const [arity, nReturn] = clean[w].match(/\d+/g).map(n => parseInt(n));
    compiled.push(
      ...hash16bin(clean[++w]),
      "len", "gth",
      (arity << 4) | nReturn,
      `\t//Func ${clean[w]}`
    );
  } else if (clean[w] == ";") {
    compiled.push(clean[w]);
  } else if ("?!".includes(clean[w])) {
    compiled.push(clean[w], 0, 0, `\t\t//${clean[w]}`);
  } else if (/^[A-Z<>+-]/.test(clean[w])) {
    const op = ops[clean[w]];
    if (op)
      compiled.push(op, `\t\t\t\t//${clean[w]}`);
    else
      compiled.push(
        ops.CALL,
        ...hash16bin(clean[w]),
        `\t\t//Call ${clean[w]}`
      );
  } else if (clean[w].startsWith("=")) {
    compiled.push(
      ops.MARK,
      ...hash16bin(clean[w].slice(1)),
      `\t\t//Mark ${clean[w]}`
    );
  } else if (/^0x\d\d$/.test(clean[w])) {
    compiled.push(
      ops.U08,
      parseInt(clean[w], 16),
      `\t\t//Push U08 ${clean[w]}`
    );
  } else if (/^\d+$/.test(clean[w])) {
    compiled.push(
      ops.I32,
      ...i32bin(parseInt(clean[w])),
      `\t//Push I32 ${clean[w]}`
    );
  } else if (clean[w] == '"') {
    const str = strings.pop().replace(/\\"/g, '"');
    compiled.push(
      ops.BLOB,
      ...u16bin(str.length),
      ...[...str].map(c => c.charCodeAt(0)),
      `\t//Push \`${str}\``
    );
  } else {
    compiled.push(ops.VAR, ...hash16bin(clean[w]), `\t\t//Var ${clean[w]}`);
  }
  compiled.push("\n");
}
const smartFind = (what, i, doCount = true) => {
  for (let l = 0, count = 0; i < compiled.length; ++i) {
    if (invisiComp.test(compiled[i])) continue;
    if (what.includes(compiled[i]) && !count--)
      return l + (compiled[i] == "!" ? 3 : (what[0] == ";" ? 1 : 0));
    if (doCount && compiled[i] == "?") ++count;
    ++l;
  }
};
{
  let i = 0, lastFuncI = 0;
  for (; i < compiled.length; ++i) {
    if ("?!".includes(compiled[i])) {
      const len = smartFind("!.;", i + 3);
      compiled[i] = ops[compiled[i]];
      [compiled[i + 1], compiled[i + 2]] = num16bin(len);
    } else if (compiled[i] == "len") {
      const len = smartFind(";", i + 3, false);
      [compiled[i], compiled[i + 1]] = num16bin(len);
    } else if (compiled[i] == ";") {
      compiled[i] = ops.RETURN;
      lastFuncI = i + 1;
    }
  }
  compiled.push(...num16bin(compiled
    .slice(lastFuncI)
    .filter(c => !invisiComp.test(c))
    .length), "\t\t\t//Entry length");
}

console.log(`//${compiled.filter(x => Number.isInteger(x)).length}B`);
console.log(
  compiled.map(x => Number.isInteger(x) ? `0x${toHex(x)}, ` : x).join("")
);
