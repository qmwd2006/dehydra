// { 'test': 'dehydra', 'input': 'numinfo.cc', 'output': 'unit_test' }

include('unit_test.js')

let r = new TestResults();

function NuminfoTestCase(type, unsigned, prec, min, max) {
  this.type = type;
  this.unsigned = unsigned;
  this.prec = prec;
  this.min = min;
  this.max = max;
}

NuminfoTestCase.prototype = new TestCase();

NuminfoTestCase.prototype.runTest = function () {
  let type = this.type;
  if (this.unsigned)
    this.assertEquals(type.isUnsigned, true);
  else
    this.assertEquals(type.isSigned, true);
  this.assertEquals(type.precision, this.prec);
  this.assertEquals(type['min'] ? type.min.value : type['min'], this.min);
  this.assertEquals(type['max'] ? type.max.value : type['max'], this.max);
}

const expected = {
  'int8var':   [ false,  8,                 '-128',                     '127' ],
  'int16var':  [ false, 16,                 '-32768',                 '32767' ],
  'int32var':  [ false, 32,            '-2147483648',            '2147483647' ],
  'int64var':  [ false, 64, '-9223372036854775808ll', '9223372036854775807ll' ],
  'uint8var':  [  true,  8,                   '0u',                    '255u' ],
  'uint16var': [  true, 16,                   '0u',                  '65535u' ],
  'uint32var': [  true, 32,                   '0u',             '4294967295u' ],
  'uint64var': [  true, 64,                 '0ull', '18446744073709551615ull' ],
  'floatvar':  [ false, 32,              undefined,                 undefined ],
  'doublevar': [ false, 64,              undefined,                 undefined ],
  'boolvar':   [  true,  1,                   '0u',                      '1u' ]
}

let tested = {}

function process_var(v) {
  const e = expected[v.name]
  if (e) {
    tested[v.name] = true;
    new NuminfoTestCase(v.type.typedef || v.type, e[0], e[1], e[2], e[3]).run(r);
  }
  else if (['enumvar', 'refvar', 'pointervar'].indexOf(v.name) >= 0) {
    // only test for relevant things here
    tested[v.name] = true;
    let tc = new TestCase();
    tc.runTest = function () {
      tc.assertTrue((v.name != 'enumvar' || v.type.isUnsigned == true) &&
		    v.type.precision > 0);
    }
    tc.run(r);
  }
}

function input_end() {
  if (r.testsRun != 14) {
    print('Error: must be 14 tests run, instead ran ' + r.testsRun);
  }
  else {
    expected['enumvar'] = expected['refvar'] = expected['pointervar'] = true;
    for (let v in expected)
      if (!tested[v]) {
	print('Error: variable ' + v + ' was left untested');
	return;
      }
    r.list();
  }
}