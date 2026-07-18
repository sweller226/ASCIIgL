#include <ASCIICraft/sound/SoundRegistry.hpp>

namespace sound {

void RegisterDefaultSounds(SoundRegistry& registry) {
    registry.Register("step.grass", {
        "res/sounds/step/grass1.ogg",
        "res/sounds/step/grass2.ogg",
        "res/sounds/step/grass3.ogg",
        "res/sounds/step/grass4.ogg",
        "res/sounds/step/grass5.ogg",
        "res/sounds/step/grass6.ogg",
    });

    registry.Register("step.gravel", {
        "res/sounds/step/gravel1.ogg",
        "res/sounds/step/gravel2.ogg",
        "res/sounds/step/gravel3.ogg",
        "res/sounds/step/gravel4.ogg",
    });

    registry.Register("step.wood", {
        "res/sounds/step/wood1.ogg",
        "res/sounds/step/wood2.ogg",
        "res/sounds/step/wood3.ogg",
        "res/sounds/step/wood4.ogg",
        "res/sounds/step/wood5.ogg",
        "res/sounds/step/wood6.ogg",
    });

    registry.Register("step.stone", {
        "res/sounds/step/stone1.ogg",
        "res/sounds/step/stone2.ogg",
        "res/sounds/step/stone3.ogg",
        "res/sounds/step/stone4.ogg",
        "res/sounds/step/stone5.ogg",
        "res/sounds/step/stone6.ogg",
    });

    registry.Register("step.sand", {
        "res/sounds/step/sand1.ogg",
        "res/sounds/step/sand2.ogg",
        "res/sounds/step/sand3.ogg",
        "res/sounds/step/sand4.ogg",
        "res/sounds/step/sand5.ogg",
    });

    registry.Register("step.snow", {
        "res/sounds/step/snow1.ogg",
        "res/sounds/step/snow2.ogg",
        "res/sounds/step/snow3.ogg",
        "res/sounds/step/snow4.ogg",
    });

    registry.Register("step.cloth", {
        "res/sounds/step/cloth1.ogg",
        "res/sounds/step/cloth2.ogg",
        "res/sounds/step/cloth3.ogg",
        "res/sounds/step/cloth4.ogg",
    });

    registry.Register("dig.grass", {
        "res/sounds/dig/grass1.ogg",
        "res/sounds/dig/grass2.ogg",
        "res/sounds/dig/grass3.ogg",
        "res/sounds/dig/grass4.ogg",
    });

    registry.Register("dig.gravel", {
        "res/sounds/dig/gravel1.ogg",
        "res/sounds/dig/gravel2.ogg",
        "res/sounds/dig/gravel3.ogg",
        "res/sounds/dig/gravel4.ogg",
    });

    registry.Register("dig.wood", {
        "res/sounds/dig/wood1.ogg",
        "res/sounds/dig/wood2.ogg",
        "res/sounds/dig/wood3.ogg",
        "res/sounds/dig/wood4.ogg",
    });

    registry.Register("dig.stone", {
        "res/sounds/dig/stone1.ogg",
        "res/sounds/dig/stone2.ogg",
        "res/sounds/dig/stone3.ogg",
        "res/sounds/dig/stone4.ogg",
    });

    registry.Register("dig.sand", {
        "res/sounds/dig/sand1.ogg",
        "res/sounds/dig/sand2.ogg",
        "res/sounds/dig/sand3.ogg",
        "res/sounds/dig/sand4.ogg",
    });

    registry.Register("dig.snow", {
        "res/sounds/dig/snow1.ogg",
        "res/sounds/dig/snow2.ogg",
        "res/sounds/dig/snow3.ogg",
        "res/sounds/dig/snow4.ogg",
    });

    registry.Register("dig.cloth", {
        "res/sounds/dig/cloth1.ogg",
        "res/sounds/dig/cloth2.ogg",
        "res/sounds/dig/cloth3.ogg",
        "res/sounds/dig/cloth4.ogg",
    });

    registry.Register("dig.coral", {
        "res/sounds/dig/coral1.ogg",
        "res/sounds/dig/coral2.ogg",
        "res/sounds/dig/coral3.ogg",
        "res/sounds/dig/coral4.ogg",
    });

    registry.Register("dig.wet_grass", {
        "res/sounds/dig/wet_grass1.ogg",
        "res/sounds/dig/wet_grass2.ogg",
        "res/sounds/dig/wet_grass3.ogg",
        "res/sounds/dig/wet_grass4.ogg",
    });

    registry.Register("music.game.calm1", "res/sounds/music/game/calm1.ogg");
    registry.Register("music.game.calm2", "res/sounds/music/game/calm2.ogg");
    registry.Register("music.game.calm3", "res/sounds/music/game/calm3.ogg");
    registry.Register("music.game.hal1", "res/sounds/music/game/hal1.ogg");
    registry.Register("music.game.hal2", "res/sounds/music/game/hal2.ogg");
    registry.Register("music.game.hal3", "res/sounds/music/game/hal3.ogg");
    registry.Register("music.game.hal4", "res/sounds/music/game/hal4.ogg");
    registry.Register("music.game.nuance1", "res/sounds/music/game/nuance1.ogg");
    registry.Register("music.game.nuance2", "res/sounds/music/game/nuance2.ogg");
    registry.Register("music.game.piano1", "res/sounds/music/game/piano1.ogg");
    registry.Register("music.game.piano2", "res/sounds/music/game/piano2.ogg");
    registry.Register("music.game.piano3", "res/sounds/music/game/piano3.ogg");
}

} // namespace sound
