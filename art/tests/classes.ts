// Classes: fields, plain methods, and get/set accessor properties.

class Counter {
  count: number;

  function increment(): void { this.count = this.count + 1; }
  function add(amount: number): void { this.count = this.count + amount; }
}

class Celsius {
  raw: number;

  get fahrenheit(): number { return this.raw * 9.0 / 5.0 + 32.0; }
  set fahrenheit(f: number) { this.raw = (f - 32.0) * 5.0 / 9.0; }
}

function main(): number {
  let fails: number = 0;

  let c: Counter = { count: 0 };
  c.increment();
  c.increment();
  c.add(10);
  if (c.count != 12) { fails = fails + 1; }

  let temp: Celsius = { raw: 0 };
  if (temp.fahrenheit != 32.0) { fails = fails + 1; }
  temp.fahrenheit = 212.0;
  if (temp.raw != 100.0) { fails = fails + 1; }

  return fails;
}
