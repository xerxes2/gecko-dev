// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2020 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.add
description: A string is parsed into the correct object when passed as the argument
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const instance = Temporal.PlainDate.from({ year: 2000, month: 5, day: 2 });
const result = instance.add("P3D");
TemporalHelpers.assertPlainDate(result, 2000, 5, "M05", 5);

TemporalHelpers.assertPlainDate(instance.add("P1M1W"), 2000, 6, "M06", 9, "calendar units");

reportCompare(0, 0);
