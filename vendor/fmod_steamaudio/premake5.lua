project "phonon_fmod"
	kind "SharedLib"
	language "C++"
	cppdialect "C++17"
	location "build"
	targetdir "bin"

	files {
		"../shared/include/phonon/**.h",
		"../shared/include/phonon_fmod/**.h",
		"src/**.h",
		"src/**.cpp",
	}

	includedirs {
		"../shared/include/phonon/",
		"../shared/include/phonon_fmod/",
		"../shared/include"
	}
	
	filter "system:Windows"
		defines {
			"IPL_OS_WINDOWS",
			"NOMINMAX"
		}

	filter "configurations:Debug"
		symbols "On"

	filter "configurations:Release"
		optimize "On"
