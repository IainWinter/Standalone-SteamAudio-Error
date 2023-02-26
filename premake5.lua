workspace "Standalone Steam Audio Error"
	configurations { "Debug", "Release" }
	platforms { "x64" }

	project "Standalone Steam Audio Error"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++17"
		location "build"
		targetdir "bin"

		files {
			"src/**.h",
			"src/**.cpp"
		}

		includedirs {
			"src",
			"vendor/shared/include",
			"vendor/fmod_steamaudio/include"
		}

		libdirs {
			"vendor/shared/bin",
			"vendor/fmod_steamaudio/bin"
		}

		links {
			"fmod",
			"fmodstudio",
			"phonon",
			"phonon_fmod",
		}
		
		filter "system:Windows"
			defines {
				"IPL_OS_WINDOWS"
			}

		filter "configurations:Debug"
			symbols "On"

		filter "configurations:Release"
			optimize "On"

include "/vendor/fmod_steamaudio"