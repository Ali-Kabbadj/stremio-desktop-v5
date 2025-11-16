#!/usr/bin/env node

/****************************************************************************
 * deploy_windows.js
 *
 * Builds a release version of the application and creates a portable .7z archive.
 *
 *
 ****************************************************************************/

const fs = require("fs");
const path = require("path");
const { execSync } = require("child_process");

// ---------------------------------------------------------------------
// Project/Layout Configuration
// ---------------------------------------------------------------------
const ARCH = process.argv.includes("--x86") ? "x86" : "x64";
const SOURCE_DIR = path.resolve(__dirname, "..");
const PROJECT_NAME = "stremio";

// --- NEW DIRECTORY STRUCTURE ---
// The 'prod' folder is used exclusively for this release build process.
const PROD_BUILD_DIR = path.join(SOURCE_DIR, "prod");
// The 'release' folder is the final output destination.
const RELEASE_DIR = path.join(SOURCE_DIR, "release");

// Paths to Additional Dependencies (These should remain correct)
const MPV_DLL =
  ARCH === "x86"
    ? path.join(SOURCE_DIR, "deps", "libmpv", "i686", "libmpv-2.dll")
    : path.join(SOURCE_DIR, "deps", "libmpv", "x86_64", "libmpv-2.dll");
const SERVER_JS = path.join(SOURCE_DIR, "deps", "server.js");
const STREMIO_RUNTIME_EXE = path.join(
  SOURCE_DIR,
  "deps",
  "stremio-runtime.exe",
);
const FFMPEG_FOLDER = path.join(SOURCE_DIR, "utils", "windows", "ffmpeg");
const MPV_FOLDER = path.join(SOURCE_DIR, "utils", "mpv", "anime4k");
const DEFAULT_SETTINGS_FOLDER = path.join(SOURCE_DIR, "utils", "stremio");

// Environment Paths
const VS_INSTALL_PATH = path.join(
  process.env.ProgramFiles,
  "Microsoft Visual Studio",
  "18",
  "Insiders",
);
const VS_DEV_CMD_PATH = path.join(
  VS_INSTALL_PATH,
  "Common7",
  "Tools",
  "VsDevCmd.bat",
);
const VCPKG_CMAKE = path.join(
  SOURCE_DIR,
  "vcpkg",
  "scripts",
  "buildsystems",
  "vcpkg.cmake",
);
const VCPKG_TRIPLET = ARCH === "x86" ? "x86-windows" : "x64-windows";

// ---------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------
(async function main() {
  try {
    console.log(`\n=== Building for ${ARCH.toUpperCase()} ===`);
    const args = process.argv.slice(2);
    const buildInstaller = args.includes("--installer");
    const buildPortable = args.includes("--portable");
    const buildConfig = "Release";

    // 1) Clean previous artifacts from prod and release folders
    console.log("--- Cleaning up previous build artifacts ---");
    safeRemove(PROD_BUILD_DIR);
    safeRemove(RELEASE_DIR);
    fs.mkdirSync(PROD_BUILD_DIR, { recursive: true });
    fs.mkdirSync(RELEASE_DIR, { recursive: true });

    // 2) Configure and build the project in the 'prod' directory
    console.log(`\n=== Configuring and Building in '${PROD_BUILD_DIR}' ===`);

    if (!fs.existsSync(VS_DEV_CMD_PATH)) {
      throw new Error(
        `Visual Studio Developer Command Prompt not found at: ${VS_DEV_CMD_PATH}`,
      );
    }
    if (!fs.existsSync(VCPKG_CMAKE)) {
      throw new Error(`VCPKG toolchain file not found at: ${VCPKG_CMAKE}`);
    }

    const cmakeConfigureCommand = [
      "cmake",
      `-S "${SOURCE_DIR}"`,
      `-B "${PROD_BUILD_DIR}"`,
      // EXPLICITLY SET THE BUILD TYPE FOR THE SINGLE-CONFIG GENERATOR
      `-DCMAKE_BUILD_TYPE=${buildConfig}`,
      `-DCMAKE_TOOLCHAIN_FILE="${VCPKG_CMAKE}"`,
      `-DVCPKG_TARGET_TRIPLET=${VCPKG_TRIPLET}`,
    ].join(" ");

    const cmakeBuildCommand = `cmake --build "${PROD_BUILD_DIR}" --config ${buildConfig}`;

    const setEnvVarCommand = `set VCPKG_VISUAL_STUDIO_PATH=${VS_INSTALL_PATH}`;
    const devShellCommand = `call "${VS_DEV_CMD_PATH}" -arch=${ARCH}`;

    const fullBuildCommand = `${devShellCommand} && ${setEnvVarCommand} && ${cmakeConfigureCommand} && ${cmakeBuildCommand}`;

    console.log(`Executing build command:\n${fullBuildCommand}\n`);
    execSync(fullBuildCommand, { stdio: "inherit" });

    // 3) Prepare the 'release' directory with all necessary files
    console.log(`\n=== Assembling files in '${RELEASE_DIR}' ===`);

    // 3a) Copy the main .exe from the production build directory
    const builtExePath = path.join(PROD_BUILD_DIR, `${PROJECT_NAME}.exe`);
    const releaseExePath = path.join(RELEASE_DIR, `${PROJECT_NAME}.exe`);
    copyFile(builtExePath, releaseExePath);

    // 3b) Copy all other required DLLs and assets
    copyFile(MPV_DLL, path.join(RELEASE_DIR, path.basename(MPV_DLL)));
    copyFile(SERVER_JS, path.join(RELEASE_DIR, path.basename(SERVER_JS)));
    copyFile(
      path.join(PROD_BUILD_DIR, "libcurl.dll"),
      path.join(RELEASE_DIR, "libcurl.dll"),
    );
    copyFile(
      path.join(PROD_BUILD_DIR, "fmt.dll"),
      path.join(RELEASE_DIR, "fmt.dll"),
    );
    copyFile(
      STREMIO_RUNTIME_EXE,
      path.join(RELEASE_DIR, "stremio-runtime.exe"),
    );
    copyFolderContents(FFMPEG_FOLDER, RELEASE_DIR);
    copyFolderContentsPreservingStructure(MPV_FOLDER, RELEASE_DIR);
    copyFolderContentsPreservingStructure(
      DEFAULT_SETTINGS_FOLDER,
      path.join(RELEASE_DIR, "portable_config"),
    );

    console.log("\n=== Release folder preparation complete. ===");

    // 4) Build the final portable archive if requested
    if (buildPortable) {
      console.log("\n--- Building Portable .7z Archive ---");
      buildPortableZip();
    } else {
      console.warn(
        "\nSkipping archive creation. Use --portable flag to build it.",
      );
    }

    console.log("\nAll done!");
  } catch (err) {
    console.error("Error in deploy_windows.js:", err);
    process.exit(1);
  }
})();

/****************************************************************************
 * Helper Functions
 ****************************************************************************/

function safeRemove(dirPath) {
  if (fs.existsSync(dirPath)) {
    fs.rmSync(dirPath, { recursive: true, force: true });
  }
}

function copyFile(src, dest) {
  if (!fs.existsSync(src)) {
    console.error(`FATAL ERROR: Cannot find required file: ${src}`);
    throw new Error(`Missing source file: ${src}`);
  }
  fs.copyFileSync(src, dest);
  console.log(`Copied: ${src} -> ${dest}`);
}

function copyFolderContents(src, dest) {
  if (!fs.existsSync(src)) {
    console.warn(`Warning: missing folder: ${src}`);
    return;
  }
  const items = fs.readdirSync(src);
  for (const item of items) {
    const srcItem = path.join(src, item);
    const destItem = path.join(dest, item);
    const stats = fs.statSync(srcItem);
    if (stats.isDirectory()) {
      copyFolderContents(srcItem, dest); // Recurse and flatten
    } else {
      copyFile(srcItem, destItem);
    }
  }
}

function copyFolderContentsPreservingStructure(src, dest) {
  if (!fs.existsSync(src)) {
    console.warn(`Warning: missing folder: ${src}`);
    return;
  }
  if (!fs.existsSync(dest)) {
    fs.mkdirSync(dest, { recursive: true });
  }
  const items = fs.readdirSync(src);
  for (const item of items) {
    const srcItem = path.join(src, item);
    const destItem = path.join(dest, item);
    const stats = fs.statSync(srcItem);
    if (stats.isDirectory()) {
      copyFolderContentsPreservingStructure(srcItem, destItem); // Recurse
    } else {
      if (!srcItem.endsWith("zip") && !srcItem.endsWith("7z")) {
        copyFile(srcItem, destItem);
      }
    }
  }
}

function getPackageVersionFromCMake() {
  const cmakeFile = path.join(SOURCE_DIR, "CMakeLists.txt");
  let version = "0.0.0";
  if (fs.existsSync(cmakeFile)) {
    const content = fs.readFileSync(cmakeFile, "utf8");
    const match = content.match(
      /project\s*\(\s*stremio\s+VERSION\s+"?([\d.]+)"?\)/i,
    );
    if (match) {
      version = match[1];
    }
  }
  return version;
}

function buildPortableZip() {
  const version = getPackageVersionFromCMake();
  const portableOutput = path.join(
    RELEASE_DIR,
    `Stremio-${version}-${ARCH}.7z`,
  );
  const fixedEdgeWebView = path.join(
    SOURCE_DIR,
    "utils",
    "windows",
    "WebviewRuntime",
    ARCH,
  );
  const portableConfigDir = path.join(RELEASE_DIR, "portable_config");

  const common7zPaths = [
    "C:\\Program Files\\7-Zip\\7z.exe",
    "C:\\Program Files (x86)\\7-Zip\\7z.exe",
  ];
  const sevenZipPath = common7zPaths.find(fs.existsSync);
  if (!sevenZipPath) {
    console.error("Error: 7-Zip executable not found.");
    process.exit(1);
  }

  console.log(`Using 7-Zip at: ${sevenZipPath}`);

  // Copy WebView2 runtime for the portable package
  copyFolderContentsPreservingStructure(fixedEdgeWebView, portableConfigDir);

  // Command to archive all contents of the release directory
  const zipCommand = `"${sevenZipPath}" a -t7z -mx=9 "${portableOutput}" "${RELEASE_DIR}\\*"`;

  try {
    console.log(`Running: ${zipCommand}`);
    execSync(zipCommand, { stdio: "inherit" });
    console.log(`\nPortable ZIP created: ${portableOutput}`);

    // Clean up the temporary WebView files from the release folder
    const portableConfigWebView = path.join(portableConfigDir, "EdgeWebView");
    if (fs.existsSync(portableConfigWebView)) {
      console.log(`\nCleaning up temporary files...`);
      safeRemove(portableConfigWebView);
    }
  } catch (error) {
    console.error("Error creating the Portable ZIP:", error);
    process.exit(1);
  }
}
