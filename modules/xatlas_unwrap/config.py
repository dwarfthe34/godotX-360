def can_build(env, platform):
    return env["tools"] and platform not in ["android", "ios", "xbox360"]


def configure(env):
    pass
