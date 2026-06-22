#!/usr/bin/env -S deno run --allow-read --allow-net

const port = Number(Deno.args[0] ?? 8000);
const root = Deno.cwd();

const contentTypes: Record<string, string> = {
  ".css": "text/css; charset=utf-8",
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".ulx": "application/octet-stream",
  ".wasm": "application/wasm",
};

Deno.serve({ port }, async (request) => {
  const url = new URL(request.url);
  const pathname = url.pathname === "/" ? "/web/player.html" : url.pathname;
  const path = safePath(pathname);
  if (!path) return new Response("not found\n", { status: 404 });

  try {
    const body = await Deno.readFile(path);
    return new Response(body, {
      headers: {
        "content-type": contentTypes[extension(path)] ??
          "application/octet-stream",
      },
    });
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      return new Response("not found\n", { status: 404 });
    }
    throw error;
  }
});

console.log(`Glupsk web player: http://localhost:${port}/`);

function safePath(pathname: string) {
  const decoded = decodeURIComponent(pathname);
  const normalized = new URL(`file://${root}${decoded}`).pathname;
  return normalized.startsWith(`${root}/`) ? normalized : undefined;
}

function extension(path: string) {
  const index = path.lastIndexOf(".");
  return index < 0 ? "" : path.slice(index);
}
