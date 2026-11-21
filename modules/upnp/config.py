def can_build(env, platform):
    return platform not in ["xbox360", "xbox360xsdk"]


def configure(env):
    pass


def get_doc_classes():
    return ["UPNP", "UPNPDevice"]


def get_doc_path():
    return "doc_classes"
