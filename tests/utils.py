from typing import List

from ragger.navigator import NavInsID, NavIns

def create_simple_nav_instructions(right_clicks: int) -> List[NavIns]:
    instructions = [NavIns(NavInsID.RIGHT_CLICK)] * right_clicks
    instructions.append(NavIns(NavInsID.BOTH_CLICK))
    return instructions
