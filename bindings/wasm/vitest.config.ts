import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    environment: 'node',
    include: ['tests/**/*.test.ts'],
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html'],
      // The tests import the tsc output, so measure that; suzume.js is the Emscripten glue.
      include: ['dist/**/*.js'],
      exclude: ['dist/suzume.js'],
    },
  },
});
